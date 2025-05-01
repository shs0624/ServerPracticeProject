#include <iostream>
#include <Windows.h>
#include <process.h>

LONG Flag;
int _result = 0;
LONG _cs = 0;

UINT SpinLockThread1(LPVOID lpThreadParameter);
UINT SpinLockThread2(LPVOID lpThreadParameter);

int main()
{
	HANDLE threadArr[2];

	HANDLE hThread1;
	HANDLE hThread2;

	UINT dwThread1Id;
	UINT dwThread2Id;
	_result = 0;

	Flag = 0;
	_cs = 0;

	threadArr[0] = (HANDLE)_beginthreadex(NULL, 0, SpinLockThread1, (LPVOID)100000000, 0, &dwThread1Id);
	threadArr[1] = (HANDLE)_beginthreadex(NULL, 0, SpinLockThread2, (LPVOID)100000000, 0, &dwThread2Id);

	WaitForMultipleObjects(2, threadArr, true, INFINITE);

	printf("result : %d\n", _result);

	return 0;
}

UINT SpinLockThread1(LPVOID lpThreadParameter)
{
	int cnt = 0;

	while (cnt < 1000000)
	{
		while (1)
		{
			if (Flag == 0)
			{
				Flag = 1;
				break;
			}
			
			/*if (InterlockedExchange(&Flag, 1) == 0)
			{
				break;
			}*/

			YieldProcessor();
		}

		if (InterlockedExchange(&_cs, 1) == 2)
		{
			DebugBreak();
		}

		_result++;


		if (InterlockedExchange(&_cs, 0) == 2)
		{
			DebugBreak();
		}

		Flag = 0;
		cnt++;
	}

	return 0;
}

UINT SpinLockThread2(LPVOID lpThreadParameter)
{
	int cnt = 0;

	while (cnt < 1000000)
	{
		while (1)
		{
			if (Flag == 0)
			{
				Flag = 1;
				break;
			}

			/*if (InterlockedExchange(&Flag, 1) == 0)
			{
				break;
			}*/

			YieldProcessor();
		}


		if (InterlockedExchange(&_cs, 2) == 1)
		{
			DebugBreak();
		}

		_result++;

		if (InterlockedExchange(&_cs, 0) == 1)
		{
			DebugBreak();
		}

		Flag = 0;
		cnt++;
	}

	return 0;
}