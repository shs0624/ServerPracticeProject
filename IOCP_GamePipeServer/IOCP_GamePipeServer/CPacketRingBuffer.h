#pragma once

class CPacketRingBuffer
{
public:
    CPacketRingBuffer(int max)
        : _head(0), _tail(0)
    {
        _capacity = max;
        _arr = (RefCountPointer*)malloc(sizeof(RefCountPointer) * _capacity);
        if (_arr == nullptr)
            DebugBreak();
    }

    ~CPacketRingBuffer() { free(_arr); }

    void Clear()
    {
        _head = 0;
        _tail = 0;
    }

    bool Empty()
    {
        if (_head == _tail)
            return true;
        else
            return false;
    }

    bool Enqueue(const RefCountPointer& p)
    {
        int tempTail = _tail;
        int next = (tempTail + 1) % _capacity;
        if (next == _head) 
            return false;

        _arr[tempTail] = p;
        _tail = next;
        return true;
    }

    bool Dequeue(RefCountPointer& out)
    {
        int head = _head;
        if (head == _tail) 
            return false;

        out = _arr[head];
        _head = (head + 1) % _capacity;
        return true;
    }

    int GetUseSize() const
    {
        int tempHead = _head;
        int tempTail = _tail;

        if (tempTail == tempHead)
            return 0;

        if (tempTail > tempHead)
        {
            return tempTail - tempHead;
        }
        else
        {
            return _capacity - (tempHead - tempTail);
        }
    }

private:
    RefCountPointer* _arr;
    int _capacity;
    int _head;
    int _tail;
};