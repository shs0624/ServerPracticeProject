#include <iostream>
#include <Windows.h>
#include <process.h>

LONG Flag;
int _result = 0;

UINT SpinLockThread(LPVOID lpThreadParameter);

int main()
{
	HANDLE threadArr[2];

	HANDLE hThread1;
	HANDLE hThread2;

	UINT dwThread1Id;
	UINT dwThread2Id;
	_result = 0;

	//threadArr[0] = (HANDLE)_beginthreadex(NULL, 0, Thread1, (LPVOID)100000000, 0, &dwThread1Id);
	//threadArr[1] = (HANDLE)_beginthreadex(NULL, 0, Thread2, (LPVOID)100000000, 0, &dwThread2Id);

	WaitForMultipleObjects(2, threadArr, true, INFINITE);

	printf("result : %d\n", _result);

	return 0;
}

UINT SpinLockThread(LPVOID lpThreadParameter)
{
	while (1)
	{
		if (InterlockedExchange(&Flag, 1) == 0)
		{
			break;
		}
		
		YieldProcessor();
	}

	Flag = 0;
}