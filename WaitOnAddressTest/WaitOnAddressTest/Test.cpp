#pragma comment(lib, "Synchronization.lib")
#include <iostream>
#include <Windows.h>
#include <process.h>

HANDLE handleArr[2];
HANDLE h1, h2;
UINT thread1ID;
UINT thread2ID;

int aCount;
int bCount;

DWORD _iLock = 0;

void Lock();
void UnLock();
UINT ThreadFunc1(LPVOID arg);



int wmain()
{
	handleArr[0] = (HANDLE)_beginthreadex(NULL, 0, ThreadFunc1, (LPVOID)1, NULL, &thread1ID);
	handleArr[1] = (HANDLE)_beginthreadex(NULL, 0, ThreadFunc1, (LPVOID)2, NULL, &thread1ID);

	WaitForMultipleObjects(2, handleArr, true, INFINITE);

	return 0;
}

void Lock()
{
	DWORD Compare = 1;

	while (1)
	{
		if (InterlockedExchange(&_iLock, 1) == 0)
		{
			return;
		}

		WaitOnAddress(&_iLock, &Compare, sizeof(DWORD), INFINITE);
	}
}

void UnLock()
{
	InterlockedExchange(&_iLock, 0);

	WakeByAddressSingle(&_iLock);
}

UINT ThreadFunc1(LPVOID arg)
{
	while (1)
	{
		Lock();

		if ((int)arg == 1)
			printf("ThreadID : %d!\n", (int)arg);
		else
			printf("ThreadID :		%d!\n", (int)arg);

		UnLock();

		//SwitchToThread();
		Sleep(0);
		Sleep(0);
		Sleep(0);
		Sleep(0);
		Sleep(0);
	}

	return 0;
}