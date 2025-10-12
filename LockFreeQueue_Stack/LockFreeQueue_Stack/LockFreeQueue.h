#pragma once
#include "CFreeList_Re.h"
//#include "MemoryPool.h"
#define LOGARR_MAX 10000

enum workType_Q
{
    Enqueue,
    Dequeue
};

template <typename T>
class LockFreeQueue
{
private:
    DWORD _size;
    DWORD _dwCount;
    DWORD _dwLogCount;
    DWORD _dwTailLogCount;
    DWORD _dwAllocCount;

    struct st_Node
    {
        T data;
        st_Node* next;
    };

    struct st_LOG
    {
        alignas(8) workType_Q type;
        st_Node* pNode;
        st_Node* head;
        st_Node* tail;
        DWORD64 _dwsize;
        DWORD64 _dwThreadID;
    };

    st_Node* _head;        // 시작노드를 포인트한다.
    st_Node* _tail;        // 마지막노드를 포인트한다.

    st_LOG _workArr[LOGARR_MAX];
    st_LOG _tailLogArr[LOGARR_MAX];
    st_Node* _allocArr[LOGARR_MAX];
    //procademy::CMemoryPool<st_Node>* _NodePool;
    procademy::CMemoryPool<st_Node>* _NodePool;
public:
    LockFreeQueue() : _NodePool(new procademy::CMemoryPool<st_Node>(100000))
    {
        _size = 0;
        _head = _NodePool->Alloc();
        _head->next = NULL;
        _tail = _head;
    }

    void Enqueue(T t)
    {
        st_Node* node = _NodePool->Alloc();
        node->data = t;
        node->next = NULL;

        DWORD allocIdx = InterlockedIncrement(&_dwAllocCount) % LOGARR_MAX;
        _allocArr[allocIdx] = node;

        DWORD localCnt = InterlockedIncrement(&_dwCount);
        st_Node* EnqueueNode = (st_Node*)((ULONGLONG)node | (ULONGLONG)localCnt << 47);
        // 이걸 넣어야지

        while (true)
        {
            // tail도 원상복귀 필요
            st_Node* tail = _tail;

            st_Node* tailPtr = (st_Node*)(0x00007fffffffffff & (ULONGLONG)tail);
            st_Node* next = tailPtr->next;

            if (next == NULL)
            {
                if (InterlockedCompareExchangePointer((PVOID*)&tailPtr->next, EnqueueNode, NULL) == next)
                {
                    DWORD logIdx = InterlockedIncrement(&_dwLogCount) % LOGARR_MAX;
                    _workArr[logIdx].type = workType_Q::Enqueue;
                    _workArr[logIdx].pNode = EnqueueNode;
                    _workArr[logIdx].head = _head;
                    _workArr[logIdx].tail = tail;
                    _workArr[logIdx]._dwsize = InterlockedIncrement(&_size);
                    _workArr[logIdx]._dwThreadID = GetCurrentThreadId();

                    if (InterlockedCompareExchangePointer((PVOID*)&_tail, EnqueueNode, tail) != tail)
                    {
                        // 실패의 경우 그 이유 추적
                        //DebugBreak();

                        st_Node* node = (st_Node*)(0x00007fffffffffff & (ULONGLONG)tailPtr->next);
                        while (node->next != NULL)
                        {
                            node = (st_Node*)(0x00007fffffffffff & (ULONGLONG)node->next);
                        }

                        InterlockedExchangePointer((PVOID*)&_tail, node);
                    }

                    break;
                }

                //if (InterlockedCompareExchangePointer((PVOID*)&_tail, EnqueueNode, tail) == tail)
                //{
                //    // 실패의 경우 그 이유 추적
                //    DWORD logIdx = _InterlockedIncrement(&_dwLogCount) % LOGARR_MAX;
                //    _workArr[logIdx].type = workType_Q::Enqueue;
                //    _workArr[logIdx].pNode = EnqueueNode;
                //    _workArr[logIdx].head = _head;
                //    _workArr[logIdx].tail = tail;
                //    _workArr[logIdx]._dwsize = _size + 1;
                //    _workArr[logIdx]._dwThreadID = GetCurrentThreadId();
                //    break;
                //}
            }
        }
    }

    int Dequeue(T& t)
    {
        DWORD localCnt = InterlockedIncrement(&_dwCount);

        while (true)
        {
            st_Node* head = _head;

            st_Node* headPtr = (st_Node*)(0x00007fffffffffff & (ULONGLONG)head);
            st_Node* next = headPtr->next;
            //T localData = ((st_Node*)(0x00007fffffffffff & (ULONGLONG)next))->data;

            if (next == NULL)
                continue;

            // 비어있다?
            if (next == NULL)
            {
                DebugBreak();
                return -1;
            }
            else
            {
                if (InterlockedCompareExchangePointer((PVOID*)&_head, next, head) == head)
                {
                    st_Node* localNode = (st_Node*)(0x00007fffffffffff & (ULONGLONG)next);

                    DWORD logIdx = InterlockedIncrement(&_dwLogCount) % LOGARR_MAX;
                    _workArr[logIdx].type = workType_Q::Dequeue;
                    _workArr[logIdx].pNode = head;
                    _workArr[logIdx].head = _head;
                    _workArr[logIdx].tail = _tail;
                    _workArr[logIdx]._dwsize = InterlockedDecrement(&_size);
                    _workArr[logIdx]._dwThreadID = GetCurrentThreadId();

                    t = localNode->data;

                    _NodePool->Free(headPtr);
                    break;
                }
            }
        }

        return 0;
    }
};