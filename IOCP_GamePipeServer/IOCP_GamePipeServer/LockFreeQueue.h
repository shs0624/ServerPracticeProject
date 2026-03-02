#pragma once
#include "TLSMemoryPool.h"
//#include "TLS_MemoryPool.h"
//#include "CFreeList_LockFree.h"
#define LOGARR_MAX 10000
//#define LOG_LOCKFREEQUEUE

enum workType_Q
{
    Enqueue,
    Dequeue
};

template <typename T>
class LockFreeQueue
{
private:
    DWORD _dwMaxSize;
    DWORD _size;
    DWORD _dwCount;
    DWORD64 _EndPointNode;
#ifdef LOG_LOCKFREEQUEUE
    DWORD _dwLogCount;
    DWORD _dwTailLogCount;
#endif

    struct st_Node
    {
        T data;
        st_Node* next;
    };

#ifdef LOG_LOCKFREEQUEUE
    struct st_LOG
    {
        alignas(8) workType_Q type;
        st_Node* pNode;
        st_Node* head;
        st_Node* tail;
        DWORD64 _dwsize;
        DWORD64 _dwThreadID;
    };
    st_LOG _workArr[LOGARR_MAX];
#endif

    st_Node* _head;        // 시작노드를 포인트한다.
    st_Node* _tail;        // 마지막노드를 포인트한다.

    static TLSMemoryPoolManager<st_Node> _NodePool;

public:
    LockFreeQueue()
    {
        _EndPointNode = (DWORD64)&_EndPointNode;
        _dwCount = 0;
#ifdef LOG_LOCKFREEQUEUE
        _dwLogCount = 0;
#endif
        _size = 0;
        _head = _NodePool.Alloc();
        _head->next = (st_Node*)_EndPointNode;
        _tail = _head;
    }

    void Clear()
    {
        _size = 0;

        while (!Empty())
        {
            Pop_Front();
        }

        st_Node* _headP = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)_head);
        _headP->next = (st_Node*)_EndPointNode;
        _tail = _head;
    }

    int Size()
    {
        return _size;
    }

    bool Empty()
    {
        st_Node* _headP = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)_head);
        if (_headP->next == (st_Node*)_EndPointNode)
            return true;

        return false;
    }

    void Enqueue(T t)
    {
        DWORD64 endPoint = _EndPointNode;
        st_Node* node = _NodePool.Alloc();
        node->data = t;
        node->next = (st_Node*)endPoint;

        ULONGLONG localCnt = (ULONGLONG)InterlockedIncrement(&_dwCount) % (USHRT_MAX + 1);
        st_Node* EnqueueNode = (st_Node*)((ULONGLONG)node | localCnt << 48);
        // 이걸 넣어야지

        // tail을 밀어줘야 한다.
        st_Node* _t = _tail;
        st_Node* _tailP = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)_t);
        if (_tailP->next != (st_Node*)endPoint)
        {
            InterlockedCompareExchangePointer((PVOID*)&_tail, _tailP->next, _t);
        }

        while (true)
        {
            // tail도 원상복귀 필요
            st_Node* tail = _tail;
            st_Node* tailPtr = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)tail);
            st_Node* next = tailPtr->next;

            if (next == (st_Node*)endPoint)
            {
                if (InterlockedCompareExchangePointer((PVOID*)&tailPtr->next, EnqueueNode, (st_Node*)endPoint) == next)
                {
                    DWORD nSize = InterlockedIncrement(&_size);
#ifdef LOG_LOCKFREEQUEUE
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
            else
                YieldProcessor();
        }
    }

    void Enqueue_NoLockFree(T t)
    {
        DWORD64 endPoint = _EndPointNode;
        st_Node* node = _NodePool.Alloc();
        node->data = t;
        node->next = (st_Node*)endPoint;

        ULONGLONG localCnt = (++_dwCount) % (USHRT_MAX + 1);
        st_Node* EnqueueNode = (st_Node*)((ULONGLONG)node | localCnt << 48);

        st_Node* tailPtr = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)_tail);
        st_Node* next = tailPtr->next;
        tailPtr->next = EnqueueNode;

        _tail = EnqueueNode;

        InterlockedIncrement(&_size);
    }

    // 하나의 스레드에서만 디큐하는 경우를 위한 함수. 한 스레드만 건드린다고 가정하고 짜봄.
    int Dequeue_NoLockFree(T& t)
    {
        st_Node* headPtr = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)_head);
        st_Node* next = headPtr->next;

        _head = next;
        InterlockedDecrement(&_size);

        st_Node* dataNode = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)next);
        t = dataNode->data;

        _NodePool.Free(headPtr);
        return 0;
    }

    int Dequeue(T& t)
    {
        DWORD64 endPoint = _EndPointNode;
        while (true)
        {
            st_Node* head = _head;

            st_Node* headPtr = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)head);
            st_Node* next = headPtr->next;

            if (next == (st_Node*)endPoint)
                continue;

            // tail을 밀어줘야 하는지 체크
            st_Node* _t = _tail;
            st_Node* _tailP = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)_t);
            if (_tailP->next != (st_Node*)endPoint)
            {
                InterlockedCompareExchangePointer((PVOID*)&_tail, _tailP->next, _t);
            }

            if (InterlockedCompareExchangePointer((PVOID*)&_head, next, head) == head)
            {
                st_Node* localNode = (st_Node*)(0x0000ffffffffffff & (ULONGLONG)next);

                DWORD nSize = InterlockedDecrement(&_size);
#ifdef LOG_LOCKFREEQUEUE
                DWORD logIdx = InterlockedIncrement(&_dwLogCount) % LOGARR_MAX;
                _workArr[logIdx].type = workType_Q::Dequeue;
                _workArr[logIdx].pNode = head;
                _workArr[logIdx].head = _head;
                _workArr[logIdx].tail = _tail;
                _workArr[logIdx]._dwsize = nSize;
                _workArr[logIdx]._dwThreadID = GetCurrentThreadId();
#endif

                t = localNode->data;

                _NodePool.Free(headPtr);
                break;
            }
            else
                YieldProcessor();
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
TLSMemoryPoolManager<typename LockFreeQueue<T>::st_Node>
LockFreeQueue<T>::_NodePool(500, 5, 20);