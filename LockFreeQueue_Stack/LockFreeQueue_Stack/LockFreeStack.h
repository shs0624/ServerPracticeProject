#include<Windows.h>
#include <iostream>
#include <unordered_set>

template <typename T>
class LockFreeStack
{
	struct st_NODE
	{
		T value;
		st_NODE* Next;
	};

public:
	LockFreeStack()
	{
		InitializeCriticalSection(&_cs);
		_StartNode = new st_NODE;
		_TopNode = _StartNode;
	}

	bool push(T data)
	{
		st_NODE* newNode = new st_NODE;
		newNode->value = data;

		st_NODE* oldTop = _TopNode;
		newNode->Next = oldTop;

		if (InterlockedCompareExchange64((__int64*)&_TopNode, (__int64)newNode, (__int64)oldTop) == (__int64)oldTop)
		{
			return true;
		}

		return false;
	}

	bool pop(T* output, void** deletePtr)
	{
		st_NODE* oldTop = _TopNode;
		st_NODE* newNode = oldTop->Next;

		if (oldTop == _StartNode)
			return false;

		if (InterlockedCompareExchange64((__int64*)&_TopNode, (__int64)newNode, (__int64)oldTop) == (__int64)oldTop)
		{
			*output = oldTop->value;
			*deletePtr = oldTop;
			delete oldTop;

			EnterCriticalSection(&_cs);
			_deletePtrSet.insert(*deletePtr);
			LeaveCriticalSection(&_cs);

			return true;
		}
		
		return false;
	}
private:
	CRITICAL_SECTION _cs;
	std::unordered_set<void*> _deletePtrSet;
	st_NODE* _TopNode;
	st_NODE* _StartNode;
};