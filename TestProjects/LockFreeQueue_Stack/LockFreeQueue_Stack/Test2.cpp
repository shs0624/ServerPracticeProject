// File: PracticeCode/LockFreeQueue_StressTest.cpp
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cassert>
#include <Windows.h>

#include "LockFreeQueue.h"

static const int kTotalValues = 10000;

int main()
{
    LockFreeQueue<int> queue;

    const unsigned int hw = max(1u, std::thread::hardware_concurrency());
    const int producerThreads = (int)hw;
    const int consumerThreads = (int)hw;

    std::vector<std::atomic<int>> counts(kTotalValues + 1);
    auto resetCounts = [&]() {
        for (int i = 0; i <= kTotalValues; ++i) counts[i].store(0, std::memory_order_relaxed);
        };

    long long iter = 0;
    while (true)
    {
        resetCounts();

        // 1) 생산: 1..kTotalValues 범위를 스레드별로 분할해 Enqueue
        std::vector<std::thread> producers;
        producers.reserve(producerThreads);
        const int perThread = (kTotalValues + producerThreads - 1) / producerThreads;

        for (int t = 0; t < producerThreads; ++t)
        {
            const int start = t * perThread + 1;
            const int end = min(kTotalValues, (t + 1) * perThread);
            if (start > end) break;

            producers.emplace_back([start, end, &queue]() {
                for (int v = start; v <= end; ++v)
                    queue.Enqueue(v);
                });
        }
        for (auto& th : producers) th.join();

        // 2) 소비: 총 kTotalValues개의 '티켓'을 선점하고, 각 티켓당 정확히 1회 Dequeue
        std::atomic<int> nextTicket{ 0 };
        std::vector<std::thread> consumers;
        consumers.reserve(consumerThreads);

        for (int t = 0; t < consumerThreads; ++t)
        {
            consumers.emplace_back([&]() {
                while (true)
                {
                    int my = nextTicket.fetch_add(1, std::memory_order_acq_rel);
                    if (my >= kTotalValues) break; // 내 할당분 없음 → 종료

                    int v = 0;
                    // 티켓을 받은 만큼 반드시 1개 Dequeue를 완료해야 함
                    queue.Dequeue(v);

                    if (v >= 1 && v <= kTotalValues)
                        counts[v].fetch_add(1, std::memory_order_relaxed);
                    else {
                        std::printf("[FAIL] Out-of-range value dequeued: %d\n", v);
                        std::fflush(stdout);
                        std::exit(1);
                    }
                }
                });
        }
        for (auto& th : consumers) th.join();

        // 3) 검증: 각 값이 정확히 1회씩 나왔는지
        bool ok = true;
        for (int v = 1; v <= kTotalValues; ++v)
        {
            int c = counts[v].load(std::memory_order_relaxed);
            if (c != 1)
            {
                std::printf("[FAIL] Value %d count = %d (expected 1)\n", v, c);
                ok = false;
                break;
            }
        }

        if (!ok)
        {
            std::printf("[FAIL] iteration %lld\n", iter);
            std::fflush(stdout);
            return 1;
        }

        if ((iter % 100) == 0)
        {
            std::printf("[PASS] iteration %lld (producers=%d, consumers=%d)\n",
                iter, producerThreads, consumerThreads);
            std::fflush(stdout);
        }

        // 다음 라운드 준비: 남아있을 리 없지만, 방어적으로 비어있지 않다면 Clear
        if (!queue.Empty())
            queue.Clear();

        ++iter;
    }

    return 0;
}