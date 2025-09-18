#pragma comment(lib,"ws2_32")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include <queue>
#include "LockFreeStack_Re.h"
#include "LockFreeQueue.h"
#include "CFreeList_Re.h"
using namespace std;

//procademy::CMemoryPool<int> _testPool(300000);
//queue<void*> _deletePrintQ;

LockFreeStack<int> _lfStack;
LockFreeQueue<int> _lfQueue;

HANDLE _pushThreadHandleArr[10];
HANDLE _popThreadHandleArr[10];
HANDLE _workerThreadHandleArr[20];
HANDLE _printThreadHandle;

unsigned int _pushThreadID[10];
unsigned int _popThreadID[10];
unsigned int _workerThreadID[20];
unsigned int _printThreadID;

unsigned int WINAPI PrintThread(LPVOID arg);
unsigned int WINAPI PushThread(LPVOID arg);
unsigned int WINAPI PopThread(LPVOID arg);
unsigned int WINAPI WorkerThread(LPVOID arg);

bool _pushEnd = false;
bool _popEnd = false;

CRITICAL_SECTION _printCS;


int wmain()
{
	InitializeCriticalSection(&_printCS);

	/*_printThreadHandle = (HANDLE)_beginthreadex(NULL, 0, PrintThread, 0, 0, &_printThreadID);
	if (_printThreadHandle == NULL)
		return 1;*/

	// 메모리 문제 확인용
	/*for (int i = 0; i < 1000000 * 20; i++)
	{
		_lfStack.push(i);
	}*/

	for (int i = 0; i < 3; i++)
	{
		_workerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, 0, 0, &_workerThreadID[i]);
		if (_workerThreadHandleArr[i] == NULL)
			return 1;
	}

	/*for (int i = 0; i < 20; i++)
	{
		_workerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, 0, 0, &_workerThreadID[i]);
		if (_workerThreadHandleArr[i] == NULL)
			return 1;
	}*/

	WaitForMultipleObjects(3, _workerThreadHandleArr, TRUE, INFINITE);

	printf("Done!");

	return 0;
}

unsigned int WINAPI PrintThread(LPVOID arg)
{
	/*int cnt = 0;
	while (1)
	{
		if (_lfStack.Log(cnt))
			cnt++;
	}*/
}

// ABA 문제 확인용
//unsigned int WINAPI WorkerThread(LPVOID arg)
//{
//	int idx = 0;
//	int output;
//	void* ptr;
//
//	while (idx < 1000000)
//	{
//		//printf("push : %d\n", idx);
//		//_PrintQ.push(make_pair(PUSH, idx));
//		_lfStack.push(idx++);
//		//_lfStack.push(idx++);
//
//		_lfStack.pop(&output, &ptr);
//		//_PrintQ.push(make_pair(POP, output));
//		//printf("pop : %p\n", ptr);
//	}
//
//	return 0;
//}

// 메모리 문제 확인용
//unsigned int WINAPI WorkerThread(LPVOID arg)
//{
//	int idx = 0;
//	int output;
//	void* ptr;
//
//	//while (idx < 1000000)
//	//{
//	//	//printf("push : %d\n", idx);
//	//	//_PrintQ.push(make_pair(PUSH, idx));
//	//	_lfStack.push(idx++);
//	//}
//
//	for (int i = 0; i < 1000000; i++)
//	{
//		_lfStack.pop(&output, &ptr);
//		//_PrintQ.push(make_pair(POP, output));
//		//printf("pop : %p\n", ptr);
//	}
//
//	return 0;
//}

// 메모리풀 테스트용
//unsigned int WINAPI WorkerThread(LPVOID arg)
//{
//	int* arr[10000];
//	int cnt = 0;
//
//	for (int i = 0; i < 10000; i++)
//	{
//		arr[i] = _testPool.Alloc();
//		*arr[i] = i;
//	}
//
//	for (int i = 0; i < 10000; i++)
//	{
//		printf("free[i] : %d\n", i, *arr[i]);
//		_testPool.Free(arr[i]);
//	}
//
//	return 0;
//}

// 락프리큐 테스트용
unsigned int WINAPI WorkerThread(LPVOID arg)
{
	int arr[3];
	int cnt = 0;

	for (int i = 0; i < 100; i++)
	{
		for (int i = 0; i < 3; i++)
		{
			_lfQueue.Enqueue(i);
		}

		for (int i = 0; i < 1; i++)
		{
			int num = _lfQueue.Dequeue(arr[i]);
			printf("ThreadID[%d] : %d\n", GetCurrentThreadId(), arr[i]);
		}
	}

	return 0;
}