#include <Windows.h>
#include <process.h>
#include <iostream>

int g_Flag[2];
int g_cs[2];
LONG g_turn;

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

	while (1)
	{
		int tFlag;
		LONG tTurn;

		g_Flag[0] = true;
		g_turn = 1;
		//if (InterlockedExchange(&g_turn, 1) == 2)
		//{
		//	// 2->1로 바뀐경우
		//	DebugBreak();
		//}

		while (1)
		{
			tFlag = g_Flag[1];
			tTurn = g_turn;
			if (tFlag == false)
				break;
			if (tTurn != 1)
				break;
		}

		_result++;
		cnt++;

		g_Flag[0] = false;
		g_turn = 0;
		//if (InterlockedExchange(&g_turn, 0) == 2)
		//{
		//	// 2->0으로 바뀐 경우
		//	DebugBreak();
		//}

		if (cnt == (int)lpThreadParameter)
			break;
	}

	return 0;
}

UINT Thread2(LPVOID lpThreadParameter)
{
	int cnt = 0;

	while (1)
	{
		int tFlag;
		LONG tTurn;

		g_Flag[1] = true;
		g_turn = 2;
		//if (InterlockedExchange(&g_turn, 2) == 1)
		//{
		//	// 1->2로 바뀐경우
		//	DebugBreak();
		//}

		while (1)
		{
			tFlag = g_Flag[0];
			tTurn = g_turn;
			if (tFlag == false)
				break;
			if (tTurn != 2)
				break;
		}

		_result++;
		cnt++;

		g_Flag[1] = false;
		g_turn = 0;
		//if (InterlockedExchange(&g_turn, 0) == 1)
		//{
		//	// 1->0로 바뀐경우
		//	DebugBreak();
		//}

		if (cnt == (int)lpThreadParameter)
			break;
	}

	return 0;
}

//// 둘 다 임계영역에 있는 모습을 확인할 수 있는 버전
//UINT Thread1(LPVOID lpThreadParameter)
//{
//	int cnt = 0;
//
//	while (1)
//	{
//		int tFlag;
//		LONG tTurn;
//
//		g_Flag[0] = true;
//		if (InterlockedExchange(&g_turn, 1) == 2)
//		{
//			// 2->1로 바뀐경우
//			DebugBreak();
//		}
//
//		while (1)
//		{
//			tFlag = g_Flag[1];
//			tTurn = g_turn;
//			if (tFlag == false)
//				break;
//			if (tTurn != 0)
//				break;
//		}
//
//		_result++;
//		cnt++;
//
//		g_Flag[0] = false;
//		if (InterlockedExchange(&g_turn, 0) == 2)
//		{
//			// 2->0으로 바뀐 경우
//			DebugBreak();
//		}
//
//		if (cnt == (int)lpThreadParameter)
//			break;
//	}
//
//	return 0;
//}
//
//UINT Thread2(LPVOID lpThreadParameter)
//{
//	int cnt = 0;
//
//	while (1)
//	{
//		int tFlag;
//		LONG tTurn;
//
//		g_Flag[1] = true;
//		if (InterlockedExchange(&g_turn, 2) == 1)
//		{
//			// 1->2로 바뀐경우
//			DebugBreak();
//		}
//
//		while (1)
//		{
//			tFlag = g_Flag[0];
//			tTurn = g_turn;
//			if (tFlag == false)
//				break;
//			if (tTurn != 1)
//				break;
//		}
//
//		_result++;
//		cnt++;
//
//		g_Flag[1] = false;
//		if (InterlockedExchange(&g_turn, 0) == 1)
//		{
//			// 1->0로 바뀐경우
//			DebugBreak();
//		}
//
//		if (cnt == (int)lpThreadParameter)
//			break;
//	}
//
//	return 0;
//}

// 임시변수로 확인용
/*
UINT Thread1(LPVOID lpThreadParameter)
{
	int cnt = 0;

	while (1)
	{
		int tFlag;
		int tTurn;

		g_Flag[0] = true;
		g_turn = 0;

		while (1)
		{
			tFlag = g_Flag[1];
			tTurn = g_turn;
			if (tFlag == false)
				break;
			if (tTurn != 0)
				break;
		}

		if (g_Flag[1] == true && (g_turn != 1 || g_Flag[0] == false))
		{
			DebugBreak();
		}

		_result++;
		cnt++;

		g_Flag[0] = false; 

		if (cnt == (int)lpThreadParameter)
			break;
	}

	return 0;
}

UINT Thread2(LPVOID lpThreadParameter)
{
	int cnt = 0;

	while (1)
	{
		int tFlag;
		int tTurn;

		g_Flag[1] = true;
		g_turn = 1;

		while (1)
		{
			tFlag = g_Flag[0];
			tTurn = g_turn;
			if (tFlag == false)
				break;
			if (tTurn != 1)
				break;
		}

		if (g_Flag[0] == true && (g_turn != 0 || g_Flag[1] == false))
		{
			DebugBreak();
		}

		_result++;
		cnt++;

		g_Flag[1] = false;

		if (cnt == (int)lpThreadParameter)
			break;
	}

	return 0;
}
*/