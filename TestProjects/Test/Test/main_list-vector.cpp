#include <iostream>
#include <vector>
#include <list>
#include <windows.h>

#define DATA_COUNT 1000    // 100만 개 (삭제 반복을 고려해 적당한 수치)

int main()
{
    std::vector<int> vec;
    std::list<int> lst;

    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);

    printf("데이터 생성 중...\n");

    for (int i = 0; i < DATA_COUNT; ++i)
    {
        vec.push_back(i);
        lst.push_back(i);
    }

    printf("\n[순회 성능 테스트]\n");

    // vector 순회
    long long sum = 0;
    QueryPerformanceCounter(&start);
    for (int i = 0; i < vec.size(); ++i)
    {
        sum += vec[i];
    }
    QueryPerformanceCounter(&end);
    printf("vector 순회 시간: %.3f ms\n", (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);

    // list 순회
    sum = 0;
    QueryPerformanceCounter(&start);
    for (auto it = lst.begin(); it != lst.end(); ++it)
    {
        sum += *it;
    }
    QueryPerformanceCounter(&end);
    printf("list 순회 시간: %.3f ms\n", (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);

    printf("\n[검색 성능 테스트 - 마지막 요소 찾기]\n");

    int target = DATA_COUNT - 1;

    // vector 검색
    QueryPerformanceCounter(&start);
    for (int i = 0; i < vec.size(); ++i)
    {
        if (vec[i] == target)
            break;
    }
    QueryPerformanceCounter(&end);
    printf("vector 검색 시간: %.3f ms\n", (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);

    // list 검색
    QueryPerformanceCounter(&start);
    for (auto it = lst.begin(); it != lst.end(); ++it)
    {
        if (*it == target)
            break;
    }
    QueryPerformanceCounter(&end);
    printf("list 검색 시간: %.3f ms\n", (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);

    printf("\n[중간 삭제 성능 테스트 - 전체 데이터 절반 삭제]\n");

    // vector 삭제
    std::vector<int> vecDel = vec;  // 원본 유지
    QueryPerformanceCounter(&start);
    for (int i = 0; i < DATA_COUNT / 10; ++i)
    {
        vecDel.erase(vecDel.begin() + vecDel.size() / 2);  // 항상 중간 삭제
    }
    QueryPerformanceCounter(&end);
    printf("vector 중간 삭제 시간: %.3f ms\n", (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);

    // list 삭제
    std::list<int> lstDel = lst;  // 원본 유지
    QueryPerformanceCounter(&start);
    for (int i = 0; i < DATA_COUNT / 10; ++i)
    {
        auto it = lstDel.begin();
        std::advance(it, lstDel.size() / 2);  // 중간 위치로 이동
        lstDel.erase(it);
    }
    QueryPerformanceCounter(&end);
    printf("list 중간 삭제 시간: %.3f ms\n", (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);

    return 0;
}
