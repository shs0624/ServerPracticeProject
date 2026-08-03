#pragma once
#include<Windows.h>
#include <iostream>
#include <queue>
#include <utility>
#include <unordered_map>
using namespace std;

enum workType
{
	PUSH,
	POP
};

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

		ULONGLONG localIdx = _IdxValue;
		st_NODE* oldTop = _TopNode;
		newNode->Next = oldTop;

		localIdx = 0x000000000001ffff & localIdx;
		localIdx = localIdx << 47;
		newNode = (st_NODE*)((ULONGLONG)newNode | localIdx);

		if (InterlockedCompareExchange64((__int64*)&_TopNode, (__int64)newNode, (__int64)oldTop) == (__int64)oldTop)
		{
			InterlockedIncrement(&_IdxValue);

			// 로그 남기기용
			//unsigned long idx = InterlockedIncrement(&_logIdx) - 1;
			//_workArr[idx] = { PUSH, newNode };
			//_workArr[idx] = newNode;

			cnt++;
			return true;
		}

		return false;
	}

	bool pop(T* output, void** deletePtr)
	{
		st_NODE* oldTop = (st_NODE*)(0x00007fffffffffff & (ULONGLONG)_TopNode);
		st_NODE* newNode = oldTop->Next;

		if (oldTop == _StartNode)
			return false;

		if (InterlockedCompareExchange64((__int64*)&_TopNode, (__int64)newNode, (__int64)oldTop) == (__int64)oldTop)
		{
			// 로그 남기기용
			//unsigned long idx = _InterlockedIncrement(&_logIdx) - 1;
			//_workArr[idx] = { POP, oldTop };
			//_workArr[idx] = oldTop;

			cnt--;

			// 진짜 oldTop 주소
			st_NODE* tempPtr = (st_NODE*)(0x00007fffffffffff & (ULONGLONG)oldTop);

			*output = tempPtr->value;
			*deletePtr = tempPtr;
			delete tempPtr;

			/**output = oldTop->value;
			*deletePtr = oldTop;
			delete oldTop;*/
			
			return true;
		}
		
		return false;
	}

	bool Log(int num)
	{
		if (num < _logIdx)
		{
			switch (_workArr[num].first)
			{
			case PUSH:
				printf("PUSH : %p\n", _workArr[num].second);
				break;
			case POP:
				printf("POP : %p\n", _workArr[num].second);
				break;
			}
			return true;
		}
		return false;
	}
private:
	int cnt = 0;
	unsigned long _logIdx = 0;
	ULONGLONG _IdxValue = 1;

	//void* _workArr[10000000];
	pair<workType, void*> _workArr[10000000];
	queue<pair<workType, void*>> _workQ;
	//queue<void*> _workQ;
	//unordered_map<void*, int> _nodeMap;
	st_NODE* _TopNode;
	st_NODE* _StartNode;
};