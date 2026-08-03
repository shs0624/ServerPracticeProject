#pragma comment(lib, "winmm.lib")
#include <iostream>
#include <process.h>
#include <Windows.h>

LONG Flag;
int _result = 0;
LONG _cs = 0;

UINT SpinLockThread1(LPVOID lpThreadParameter);
UINT SpinLockThread2(LPVOID lpThreadParameter);
void Unlock();

DWORD startTime;

int wmain(void)
{
	timeBeginPeriod(1);

	startTime = timeGetTime();

	HANDLE threadArr[2];

	UINT dwThread1Id;
	UINT dwThread2Id;
	_result = 0;

	Flag = 0;
	_cs = 0;

	threadArr[0] = (HANDLE)_beginthreadex(NULL, 0, SpinLockThread1, (LPVOID)100000000, 0, &dwThread1Id);
	threadArr[1] = (HANDLE)_beginthreadex(NULL, 0, SpinLockThread2, (LPVOID)100000000, 0, &dwThread2Id);

	WaitForMultipleObjects(2, threadArr, true, INFINITE);

	DWORD useTime = timeGetTime() - startTime;

	printf("result : %d # useTime : %d\n", _result, useTime);

	return 0;
}

UINT SpinLockThread1(LPVOID lpThreadParameter)
{
	for (int i = 0; i < 1000000; i++)
	{
		while (1)
		{
			if (InterlockedExchange(&_cs, 1) == 0)
				break;

			//YieldProcessor();
		}

		_result++;

		Unlock();
	}

	return 0;
}

UINT SpinLockThread2(LPVOID lpThreadParameter)
{
	for (int i = 0; i < 1000000; i++)
	{
		while (1)
		{
			if (InterlockedExchange(&_cs, 2) == 0)
				break;

			//YieldProcessor();
		}

		_result++;

		Unlock();
	}

	return 0;
}

void Unlock()
{
	InterlockedExchange(&_cs, 0);
}