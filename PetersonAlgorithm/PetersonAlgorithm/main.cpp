#include <Windows.h>
#include <process.h>
#include <iostream>

int g_Flag[2];
int g_cs[2];
int g_turn;

int _result = 0;

UINT Thread1(LPVOID lpThreadParameter);
UINT Thread2(LPVOID lpThreadParameter);

int main()
{
	HANDLE threadArr[2];

	HANDLE hThread1;
	HANDLE hThread2;

	UINT dwThread1Id;
	UINT dwThread2Id;
	_result = 0;	

	threadArr[0] = (HANDLE)_beginthreadex(NULL, 0, Thread1, (LPVOID)100000000, 0, &dwThread1Id);
	threadArr[1] = (HANDLE)_beginthreadex(NULL, 0, Thread2, (LPVOID)100000000, 0, &dwThread2Id);

	WaitForMultipleObjects(2, threadArr, true, INFINITE);

	printf("result : %d\n", _result);

	return 0;
}

UINT Thread1(LPVOID lpThreadParameter)
{
	int cnt = 0;
	int errCnt = 0;

	while (1)
	{
		g_Flag[0] = true;
		g_turn = 0;

		while (1)
		{
			int tFlag = g_Flag[1];
			int tTurn = g_turn;
			if (tFlag == false)
				break;
			if (g_turn != 0)
				break;
		}

		//g_cs[0] = true;

		if (g_Flag[1] == true && (g_turn != 1 || g_Flag[0] == false))
		{
			errCnt++;
			DebugBreak();
		}

		//if (g_cs[0] == true && g_cs[1] == true)
		//{
		//	DebugBreak();
		//}

		_result++;
		cnt++;

		//g_cs[0] = false;

		g_Flag[0] = false; 

		if (cnt == (int)lpThreadParameter)
			break;
	}

	printf("errCnt Thread1 : %d\n", errCnt);

	return 0;
}

UINT Thread2(LPVOID lpThreadParameter)
{
	int cnt = 0;
	int errCnt = 0;

	while (1)
	{
		g_Flag[1] = true;
		g_turn = 1;

		while (1)
		{
			if (g_Flag[0] == false)
				break;
			if (g_turn != 1)
				break;
		}

		//g_cs[1] = true;

		if (g_Flag[0] == true && (g_turn != 0 || g_Flag[1] == false))
		{
			errCnt++;
			DebugBreak();
		}

		//if (g_cs[0] == true && g_cs[1] == true)
		//{
		//	DebugBreak();
		//}

		_result++;
		cnt++;

		//g_cs[1] = false;

		g_Flag[1] = false;

		if (cnt == (int)lpThreadParameter)
			break;
	}

	printf("errCnt Thread2 : %d\n", errCnt);

	return 0;
}