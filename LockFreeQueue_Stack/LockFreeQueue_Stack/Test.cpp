#pragma comment(lib,"ws2_32")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include <queue>
#include "LockFreeStack.h"
using namespace std;

enum printType
{
	PUSH,
	POP
};

LockFreeStack<int> _lfStack;
queue<pair<printType, int>> _PrintQ;
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

CRITICAL_SECTION _cs;


int wmain()
{
	/*for (int i = 0; i < 5; i++)
	{
		_pushThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, PushThread, 0, 0, &_pushThreadID[i]);
		if (_pushThreadHandleArr[i] == NULL)
			return 1;
	}

	for (int i = 0; i < 5; i++)
	{
		_popThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, PopThread, 0, 0, &_popThreadID[i]);
		if (_popThreadHandleArr[i] == NULL)
			return 1;
	}*/
	InitializeCriticalSection(&_cs);

	/*_printThreadHandle = (HANDLE)_beginthreadex(NULL, 0, PrintThread, 0, 0, &_printThreadID);
	if (_printThreadHandle == NULL)
		return 1;*/

	for (int i = 0; i < 10; i++)
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
	while (1)
	{
		EnterCriticalSection(&_cs);
		if (!_PrintQ.empty())
		{
			printType type = _PrintQ.front().first;
			int num = _PrintQ.front().second;
			_PrintQ.pop();

			if(type == PUSH)
				printf("push : %d\n", num);
			else
				printf("pop : %d\n", num);
		}
		LeaveCriticalSection(&_cs);
	}
}

//unsigned int WINAPI WorkerThread(LPVOID arg)
//{
//	int idx = 0;
//	int output;
//	while (idx < 1000000)
//	{
//		//printf("push : %d\n", idx);
//		_PrintQ.push(make_pair(PUSH, idx));
//		_lfStack.push(idx++);
//		
//
//		//printf("push : %d\n", idx);
//		_PrintQ.push(make_pair(PUSH, idx));
//		_lfStack.push(idx++);
//		
//
//		_lfStack.pop(&output);
//		_PrintQ.push(make_pair(POP, output));
//		//printf("pop : %d\n", output);
//
//		_lfStack.pop(&output);
//		_PrintQ.push(make_pair(POP, output));
//		//printf("pop : %d\n", output);
//	}
//	
//	return 0;
//}

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

//unsigned int WINAPI PushThread(LPVOID arg)
//{
//	for (int i = 0; i < 100000; i++)
//	{
//		_lfStack.push(i);
//		//printf("push : %d\n", i);
//	}
//
//	return 0;
//}
//
//unsigned int WINAPI PopThread(LPVOID arg)
//{
//	for (int i = 0; i < 100000; i++)
//	{
//		int num;
//		_lfStack.pop(&num);
//		//printf("pop : %d\n", num);
//	}
//
//	_popEnd = true;
//	return 0;
//}
