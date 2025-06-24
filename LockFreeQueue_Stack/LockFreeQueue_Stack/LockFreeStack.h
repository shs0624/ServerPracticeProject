#include<Windows.h>

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

	bool pop(T* output)
	{
		st_NODE* oldTop = _TopNode;
		st_NODE* newNode = oldTop->Next;

		if (oldTop == _StartNode)
			return false;

		if (InterlockedCompareExchange64((__int64*)&_TopNode, (__int64)newNode, (__int64)oldTop) == (__int64)oldTop)
		{
			*output = oldTop->value;
			delete oldTop;
			return true;
		}
		
		return false;
	}
private:
	st_NODE* _TopNode;
	st_NODE* _StartNode;
};