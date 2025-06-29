#pragma comment(lib,"ws2_32")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include <queue>
#include "LockFreeStack.h"
using namespace std;

LockFreeStack<int> _lfStack;
queue<void*> _deletePrintQ;

HANDLE _pushThreadHandleArr[10];
HANDLE _popThreadHandleArr[10];
HANDLE _workerThreadHandleArr[10];
HANDLE _printThreadHandle;

unsigned int _pushThreadID[10];
unsigned int _popThreadID[10];
unsigned int _workerThreadID[10];
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

	for (int i = 0; i < 20; i++)
	{
		_workerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, 0, 0, &_workerThreadID[i]);
		if (_workerThreadHandleArr[i] == NULL)
			return 1;
	}

	while (1)
	{

	}

	return 0;
}

unsigned int WINAPI PrintThread(LPVOID arg)
{
	int cnt = 0;
	while (1)
	{
		if (_lfStack.Log(cnt))
			cnt++;
	}
}

// ABA 문제 확인용
unsigned int WINAPI WorkerThread(LPVOID arg)
{
	int idx = 0;
	int output;
	void* ptr;

	while (idx < 1000000)
	{
		//printf("push : %d\n", idx);
		//_PrintQ.push(make_pair(PUSH, idx));
		_lfStack.push(idx++);

		_lfStack.pop(&output, &ptr);
		//_PrintQ.push(make_pair(POP, output));
		//printf("pop : %p\n", ptr);
	}

	return 0;
}

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
