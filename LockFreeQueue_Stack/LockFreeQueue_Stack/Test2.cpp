//#include <iostream>
//#include <thread>
//#include <vector>
//#include <atomic>
//#include <string>
//#include <stdexcept> // std::runtime_error
//#include <numeric>   // std::iota
//
//// ------------------------------------------------------------------
//// [ 사용자 구현 영역 ]
//// 여기에 직접 만드신 락프리 큐 헤더를 include 하거나 클래스를 정의하세요.
//#include "LockFreeQueue.h"
//// ------------------------------------------------------------------
//
//
//// --- 테스트 설정 ---
//const int NUM_THREADS = 5;
//const int ITEMS_PER_THREAD = 10000; // 스레드당 인큐할 아이템 수
//const int TOTAL_ITEMS = NUM_THREADS * ITEMS_PER_THREAD;
//
//// --- 테스트용 전역 객체 (힙 할당 최소화) ---
//
//// 테스트할 큐 (전역)
//LockFreeQueue<int> g_queue;
//
//// 카운터
//std::atomic<int> g_enqueue_counter(0);
//std::atomic<int> g_dequeue_counter(0);
//
//// 1:1 검증을 위한 상태 배열 (가장 핵심적인 자료구조)
//// 0: Initial, 1: Enqueued, 2: Dequeued
//std::vector<std::atomic<int>> g_item_states;
//
//// 테스트 실패 시 즉시 모든 스레드에 알리기 위한 플래그
//std::atomic<bool> g_test_failed(false);
//
//// 실패 원인을 저장 (여러 스레드가 동시에 쓸 수 있으므로 atomic_flag로 보호)
//std::string g_fail_reason = "";
//std::atomic_flag g_fail_lock = ATOMIC_FLAG_INIT;
//
//
///**
// * @brief 테스트 실패 시 호출되는 함수 (최초 1회만 기록)
// */
//void ReportFailure(const std::string& reason)
//{
//    // atomic_flag를 스핀락처럼 사용하여 최초의 실패 원인만 기록
//    if (!g_test_failed.load(std::memory_order_relaxed) &&
//        !g_fail_lock.test_and_set(std::memory_order_acquire))
//    {
//        g_fail_reason = reason;
//        g_test_failed.store(true, std::memory_order_relaxed);
//        // g_fail_lock은 해제하지 않음 (다른 스레드가 덮어쓰지 못하도록)
//    }
//}
//
///**
// * @brief 작업자 스레드 함수 (MPMC)
// */
//void WorkerThread()
//{
//    // 전체 디큐 카운터가 TOTAL_ITEMS에 도달하거나, 실패가 감지되면 중지
//    while (g_dequeue_counter.load(std::memory_order_relaxed) < TOTAL_ITEMS &&
//        !g_test_failed.load(std::memory_order_relaxed))
//    {
//        // 1. 인큐 시도
//        int item_id = g_enqueue_counter.fetch_add(1, std::memory_order_relaxed);
//
//        if (item_id < TOTAL_ITEMS)
//        {
//            // 상태 0 -> 1 로 변경 (인큐됨)
//            g_item_states[item_id].store(1, std::memory_order_release);
//            g_queue.Enqueue(item_id);
//        }
//
//        // 2. 디큐 시도
//        int dequeued_item;
//        if (g_queue.Dequeue(dequeued_item))
//        {
//            // 디큐 성공
//            if (dequeued_item < 0 || dequeued_item >= TOTAL_ITEMS) {
//                // 오류: 큐에서 유효하지 않은 값(손상된 데이터)이 나옴
//                ReportFailure("오류: 큐에서 손상된 데이터(범위 초과)가 Dequeue됨: " + std::to_string(dequeued_item));
//                continue;
//            }
//
//            // 상태 1 -> 2 로 변경 시도 (디큐됨)
//            int expected_state = 1;
//            bool exchanged = g_item_states[dequeued_item].compare_exchange_strong(
//                expected_state,
//                2, // 2로 변경
//                std::memory_order_acq_rel,
//                std::memory_order_relaxed
//            );
//
//            if (exchanged) {
//                // 성공: (1 -> 2)
//                g_dequeue_counter.fetch_add(1, std::memory_order_relaxed);
//            }
//            else {
//                // 실패: 상태가 1이 아니었음
//                if (expected_state == 0) {
//                    ReportFailure("오류: Enqueue된 적 없는 아이템이 Dequeue됨: " + std::to_string(dequeued_item));
//                }
//                else if (expected_state == 2) {
//                    ReportFailure("오류: 아이템이 중복 Dequeue됨: " + std::to_string(dequeued_item));
//                }
//            }
//        }
//        else {
//            // (선택) 큐가 비어있었음 (디큐 실패)
//            std::this_thread::yield();
//        }
//
//        // 3. 인큐/디큐 작업이 모두 끝난 스레드가 루프를 계속 돌며
//        //    다른 스레드를 기다려야 할 때 CPU 과다 사용 방지
//        if (item_id >= TOTAL_ITEMS &&
//            g_dequeue_counter.load(std::memory_order_relaxed) < TOTAL_ITEMS)
//        {
//            std::this_thread::yield();
//        }
//    }
//}
//
///**
// * @brief 매 배치 시작 전, 모든 전역 상태를 초기화
// */
//void ResetBatch()
//{
//    g_enqueue_counter.store(0, std::memory_order_relaxed);
//    g_dequeue_counter.store(0, std::memory_order_relaxed);
//    g_test_failed.store(false, std::memory_order_relaxed);
//
//    // 실패 원인 리셋
//    g_fail_reason.clear();
//    g_fail_lock.clear(std::memory_order_release);
//
//    // 큐 비우기 (이전 배치의 실패로 아이템이 남아있을 수 있음)
//    int dummy;
//    while (g_queue.Dequeue(dummy));
//
//    // 상태 배열 초기화 (0으로)
//    for (int i = 0; i < TOTAL_ITEMS; ++i) {
//        g_item_states[i].store(0, std::memory_order_relaxed);
//    }
//}
//
///**
// * @brief 배치가 끝난 후, 1:1 대응을 검증
// */
//bool ValidateBatch(long long batch_num)
//{
//    // 1. 스레드 내부에서 이미 실패가 감지되었는지 확인
//    if (g_test_failed.load(std::memory_order_acquire)) {
//        std::cerr << "\n!!! [배치 " << batch_num << " 실패] (스레드 실행 중 감지)" << std::endl;
//        std::cerr << "   - 원인: " << g_fail_reason << std::endl;
//        return false;
//    }
//
//    // 2. 최종 카운터 검증
//    if (g_dequeue_counter.load() != TOTAL_ITEMS) {
//        std::cerr << "\n!!! [배치 " << batch_num << " 실패] : 최종 Dequeue 카운트 불일치" << std::endl;
//        std::cerr << "   - 예상: " << TOTAL_ITEMS << ", 실제: " << g_dequeue_counter.load() << std::endl;
//        return false;
//    }
//
//    // (인큐 카운터는 TOTAL_ITEMS보다 클 수 있음(스레드가 fetch_add 경쟁하므로), 
//    //  하지만 g_item_states 검증으로 대체 가능)
//
//    // 3. 큐가 비었는지 검증
//    int dummy;
//    if (g_queue.Dequeue(dummy)) {
//        std::cerr << "\n!!! [배치 " << batch_num << " 실패] : 테스트 종료 후 큐가 비어있지 않음! (아이템: " << dummy << ")" << std::endl;
//        return false;
//    }
//
//    // 4. (가장 중요) 모든 아이템이 Dequeue(상태 2) 되었는지 검증
//    for (int i = 0; i < TOTAL_ITEMS; ++i) {
//        int final_state = g_item_states[i].load(std::memory_order_acquire);
//        if (final_state != 2) {
//            std::cerr << "\n!!! [배치 " << batch_num << " 실패] : 아이템 " << i << "의 최종 상태 오류" << std::endl;
//            if (final_state == 0) {
//                std::cerr << "   - 원인: 아이템이 Enqueue조차 되지 않음 (카운터 오류 가능성)" << std::endl;
//            }
//            else if (final_state == 1) {
//                std::cerr << "   - 원인: 아이템이 Enqueue되었으나 Dequeue되지 않음 (아이템 유실)" << std::endl;
//            }
//            return false;
//        }
//    }
//
//    return true; // 모든 검증 통과
//}
//
//
//int main()
//{
//    // 상태 배열을 처음에 단 한 번만 할당합니다.
//    try {
//        g_item_states.resize(TOTAL_ITEMS);
//    }
//    catch (const std::bad_alloc& e) {
//        std::cerr << "오류: 상태 배열(" << TOTAL_ITEMS << "개) 할당 실패. " << e.what() << std::endl;
//        return 1;
//    }
//
//    long long batch_num = 0;
//    std::cout << "무한 경합 테스트 시작 (Lightweight 버전, " << TOTAL_ITEMS << "개 아이템/배치)...\n"
//        << "오류 발생 시 또는 Ctrl+C로 중지됩니다.\n"
//        << "100 배치 성공 시 '.'이 출력됩니다." << std::endl;
//
//    std::vector<std::thread> threads;
//
//    try {
//        // 무한 루프
//        while (true)
//        {
//            // 1. 배치 상태 초기화 (힙 할당 없음)
//            ResetBatch();
//
//            threads.clear();
//
//            // 2. 스레드 실행
//            for (int i = 0; i < NUM_THREADS; ++i) {
//                threads.emplace_back(WorkerThread);
//            }
//
//            // 모든 스레드가 이 배치를 완료할 때까지 대기
//            for (auto& t : threads) {
//                t.join();
//            }
//
//            // 3. 검증
//            if (!ValidateBatch(batch_num))
//            {
//                break; // 검증 실패 시 루프 탈출
//            }
//
//            // 4. 다음 배치 준비
//            batch_num++;
//            if (batch_num % 100 == 0) {
//                std::cout << "." << std::flush;
//            }
//        } // end while(true)
//
//    }
//    catch (const std::exception& e) {
//        std::cerr << "\n!!! 테스트 중 예외 발생: " << e.what() << std::endl;
//        return 2;
//    }
//
//    std::cerr << "\n❌ [테스트 중단] 락프리 큐 구현에 오류가 감지되었습니다." << std::endl;
//    return 1; // 오류로 종료
//}