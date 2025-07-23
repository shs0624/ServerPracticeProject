#pragma comment(lib,"winmm.lib")
#include <iostream>
#include <Windows.h>
#include <process.h>
#define loopCount 10000000

LONG Flag;
int _result = 0;
LONG _cs = 0;

void Unlock();
UINT SpinLockThread1(LPVOID lpThreadParameter);
UINT SpinLockThread2(LPVOID lpThreadParameter);

DWORD _startTime;

int main()
{
	timeBeginPeriod(1);

	HANDLE threadArr[2];

	HANDLE hThread1;
	HANDLE hThread2;

	UINT dwThread1Id;
	UINT dwThread2Id;
	_result = 0;

	Flag = 0;
	_cs = 0;

	_startTime = timeGetTime();

	threadArr[0] = (HANDLE)_beginthreadex(NULL, 0, SpinLockThread1, (LPVOID)100000000, 0, &dwThread1Id);
	threadArr[1] = (HANDLE)_beginthreadex(NULL, 0, SpinLockThread2, (LPVOID)100000000, 0, &dwThread2Id);

	WaitForMultipleObjects(2, threadArr, true, INFINITE);

	DWORD useTime = timeGetTime() - _startTime;

	printf("result : %d # useTime : %d\n", _result, useTime);

	timeEndPeriod(1);

	return 0;
}

void Unlock()
{
	//InterlockedExchange(&Flag, 0);
	Flag = 0;
}

UINT SpinLockThread1(LPVOID lpThreadParameter)
{
	for (int i = 0; i < loopCount; i++)
	{
		//YieldProcessor();

		while (1)
		{
			if (InterlockedExchange(&Flag, 1) == 0)
			{
				break;
			}

			YieldProcessor();
		}

		_result++;

		Unlock();
	}
	
	return 0;
}

UINT SpinLockThread2(LPVOID lpThreadParameter)
{
	for (int i = 0; i < loopCount; i++)
	{
		//YieldProcessor();

		while (1)
		{
			if (InterlockedExchange(&Flag, 2) == 0)
			{
				break;
			}

			YieldProcessor();
		}

		_result++;

		Unlock();
	}

	return 0;
}