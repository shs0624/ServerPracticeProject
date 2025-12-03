#pragma once
//#include "TLSMemoryPool.h"
#include "TLS_MemoryPool.h"
//#include "CFreeList_LockFree.h"
#define LOGARR_MAX 10000
#define LOGGING

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
    st_Node* _allocArr[LOGARR_MAX];
    static procademy::MemoryPool_TLS<st_Node> _NodePool;
    //static procademy::MemoryPool_TLS<st_Node>* _NodePool;
    //procademy::CMemoryPool_LockFree<st_Node>* _NodePool;
    //procademy::CMemoryPool<st_Node>* _NodePool;
    //TLSMemoryPoolManager<st_Node>* _NodePool;

public:
    //LockFreeQueue() :_NodePool(new TLSMemoryPoolManager<st_Node>(500, 5, 10))
    LockFreeQueue()// : _NodePool(new procademy::MemoryPool_TLS<st_Node>(500, false))
    {
        _size = 0;
        _head = _NodePool.Alloc();
        _head->next = NULL;
        _tail = _head;
    }

    void Clear()
    {
        _size = 0;

        /*
        st_Node* _headP = (st_Node*)(0x00007fffffffffff & (ULONGLONG)_head);
        while (_headP->next != NULL)
        {
            _NodePool->Free(_headP);
            _headP = _headP->next;
        }*/
        while (!Empty())
        {
            Pop_Front();
        }

        st_Node* _headP = (st_Node*)(0x00007fffffffffff & (ULONGLONG)_head);
        _headP->next = NULL;
        _tail = _head;
    }

    int Size()
    {
        return _size;
    }

    bool Empty()
    {
        st_Node* _headP = (st_Node*)(0x00007fffffffffff & (ULONGLONG)_head);
        if (_headP->next == NULL)
            return true;

        return false;
    }

    void Enqueue(T t)
    {
        st_Node* node = _NodePool.Alloc();
        node->data = t;
        node->next = NULL;

        DWORD allocIdx = InterlockedIncrement(&_dwAllocCount) % LOGARR_MAX;
        _allocArr[allocIdx] = node;

        DWORD localCnt = InterlockedIncrement(&_dwCount);
        st_Node* EnqueueNode = (st_Node*)((ULONGLONG)node | (ULONGLONG)localCnt << 47);
        // 이걸 넣어야지

        // tail을 밀어줘야 한다.
        st_Node* _t = _tail;
        st_Node* _tailP = (st_Node*)(0x00007fffffffffff & (ULONGLONG)_t);
        if (_tailP->next != NULL)
        {
            InterlockedCompareExchangePointer((PVOID*)&_tail, _tailP->next, _t);
        }

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
                    DWORD nSize = InterlockedIncrement(&_size);
#ifdef LOGGING
                    DWORD logIdx = InterlockedIncrement(&_dwLogCount) % LOGARR_MAX;
                    _workArr[logIdx].type = workType_Q::Enqueue;
                    _workArr[logIdx].pNode = EnqueueNode;
                    _workArr[logIdx].head = _head;
                    _workArr[logIdx].tail = _tail;
                    _workArr[logIdx]._dwsize = nSize;
                    _workArr[logIdx]._dwThreadID = GetCurrentThreadId();
#endif

                    InterlockedCompareExchangePointer((PVOID*)&_tail, EnqueueNode, tail);
                    break;
                }
            }
        }
    }

    int Dequeue(T& t)
    {
        DWORD localCnt = InterlockedIncrement(&_dwCount);

        while (true)
        {
            T localData;
            st_Node* head = _head;

            st_Node* headPtr = (st_Node*)(0x00007fffffffffff & (ULONGLONG)head);
            st_Node* next = headPtr->next;

            // 데이터 미리 뽑아두기
            /*if(next != NULL)
                localData = ((st_Node*)(0x00007fffffffffff & (ULONGLONG)next))->data;*/

            if (next == NULL)
                continue;

            // tail을 밀어줘야 하는지 체크
            st_Node* _t = _tail;
            st_Node* _tailP = (st_Node*)(0x00007fffffffffff & (ULONGLONG)_t);
            if (_tailP->next != NULL)
            {
                InterlockedCompareExchangePointer((PVOID*)&_tail, _tailP->next, _t);
            }

            if (InterlockedCompareExchangePointer((PVOID*)&_head, next, head) == head)
            {
                st_Node* localNode = (st_Node*)(0x00007fffffffffff & (ULONGLONG)next);
                localData = localNode->data;

                DWORD nSize = InterlockedDecrement(&_size);
#ifdef LOGGING
                DWORD logIdx = InterlockedIncrement(&_dwLogCount) % LOGARR_MAX;
                _workArr[logIdx].type = workType_Q::Dequeue;
                _workArr[logIdx].pNode = head;
                _workArr[logIdx].head = _head;
                _workArr[logIdx].tail = _tail;
                _workArr[logIdx]._dwsize = nSize;
                _workArr[logIdx]._dwThreadID = GetCurrentThreadId();
#endif

                t = localData;

                _NodePool.Free(headPtr);
                break;
            }
        }

        return 0;
    }

    // 값을 뽑을 필요없이 그냥 뺄 때
    void Pop_Front()
    {
        T t;

        Dequeue(t);
    }

};

template <typename T>
procademy::MemoryPool_TLS<typename LockFreeQueue<T>::st_Node>
LockFreeQueue<T>::_NodePool(5000, false);