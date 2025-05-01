#include <Windows.h>
#include <process.h>
#include <iostream>

int g_Flag[2];
int g_cs[2];
int g_FlagVal = 0;
LONG g_turn;
LONG g_lock;

int _result = 0;

CRITICAL_SECTION cs;

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

	InitializeCriticalSection(&cs);
	threadArr[0] = (HANDLE)_beginthreadex(NULL, 0, Thread1, (LPVOID)100000000, 0, &dwThread1Id);
	threadArr[1] = (HANDLE)_beginthreadex(NULL, 0, Thread2, (LPVOID)100000000, 0, &dwThread2Id);

	WaitForMultipleObjects(2, threadArr, true, INFINITE);

	printf("result : %d\n", _result);

	return 0;
}

UINT Thread1(LPVOID lpThreadParameter)
{
	int cnt = 0;
	EnterCriticalSection(&cs);

	while (1)
	{
		LONG tFlag;
		LONG tTurn;

		g_Flag[0] = true; // store Flag[0]
		g_turn = 0; //store g_turn

		while (1)
		{
			tTurn = g_turn; // load g_turn, store tTurn
			tFlag = g_Flag[1]; // load Flag[1], store tFlag
			
			if (tFlag == false) // load tFlag  
				break;
			if (tTurn != 0) // load tTurn
				break;
		}

		if (InterlockedExchange(&g_lock, 1) == 2)
		{
			// 2->1로 바뀐경우
			DebugBreak();
		}

		_result++;

		if (InterlockedExchange(&g_lock, 0) == 2)
		{
			// 2->0로 바뀐경우
			DebugBreak();
		}
		g_Flag[0] = false;

		cnt++;
		if (cnt == (int)lpThreadParameter)
			break;
	}

	return 0;
}

UINT Thread2(LPVOID lpThreadParameter)
{
	int cnt = 0;
	Sleep(500);
	EnterCriticalSection(&cs);

	while (1)
	{
		LONG tFlag;
		LONG tTurn;

		g_Flag[1] = true;
		g_turn = 1;

		_Atomic_thread_fence(_Atomic_memory_order_seq_cst);
		while (1)
		{
			tTurn = g_turn;
			tFlag = g_Flag[0];

			if (tFlag == false)
				break;
			if (tTurn != 1)
				break;
		}

		if (InterlockedExchange(&g_lock, 2) == 1)
		{
			// 1->2로 바뀐 경우
			DebugBreak();
		}

		_result++;

		if (InterlockedExchange(&g_lock, 0) == 1)
		{
			// 1->0로 바뀐경우
			DebugBreak();
		} 
		g_Flag[1] = false;

		cnt++;
		if (cnt == (int)lpThreadParameter)
			break;
	}

	return 0;
}

// Yield Processor?
//UINT Thread1(LPVOID lpThreadParameter)
//{
//	int cnt = 0;
//
//	while (1)
//	{
//		LONG tFlag;
//		LONG tTurn;
//
//		g_Flag[0] = true; // store Flag[0]
//		g_turn = 0; //store g_turn
//		
//		while (1)
//		{
//			YieldProcessor();
//			tFlag = g_Flag[1]; // load Flag[1], store tFlag
//			tTurn = g_turn; // load g_turn, store tTurn
//			if (tFlag == false) // load tFlag  
//				break;
//			if (tTurn != 0) // load tTurn
//				break;
//		}
//
//		if (InterlockedExchange(&g_lock, 1) == 2)
//		{
//			// 2->1로 바뀐경우
//			DebugBreak();
//		}
//
//		_result++;
//
//		if (InterlockedExchange(&g_lock, 0) == 2)
//		{
//			// 2->0로 바뀐경우
//			DebugBreak();
//		}
//		g_Flag[0] = false;
//
//		cnt++;
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
//		LONG tFlag;
//		LONG tTurn;
//
//		g_Flag[1] = true;
//		g_turn = 1;
//
//		while (1)
//		{
//			YieldProcessor();
//			tFlag = g_Flag[0];
//			tTurn = g_turn;
//			if (tFlag == false)
//				break;
//			if (tTurn != 1)
//				break;
//		}
//
//		if (InterlockedExchange(&g_lock, 2) == 1)
//		{
//			// 1->2로 바뀐 경우
//			DebugBreak();
//		}
//
//		_result++;
//
//		if (InterlockedExchange(&g_lock, 0) == 1)
//		{
//			// 1->0로 바뀐경우
//			DebugBreak();
//		}
//		g_Flag[1] = false;
//
//		cnt++;
//		if (cnt == (int)lpThreadParameter)
//			break;
//	}
//
//	return 0;
//}


//비트연산 포기
//UINT Thread1(LPVOID lpThreadParameter)
//{
//	int cnt = 0;
//
//	while (1)
//	{
//		int tFlag;
//		LONG tTurn;
//
//		g_FlagVal = (g_FlagVal | 0x01);
//		g_turn = 0; //store g_turn
//
//		std::atomic_thread_fence(std::memory_order_seq_cst);
//		while (1)
//		{
//			tFlag = (g_FlagVal & 0x02); // 2번 비트가 켜졌는가?
//			tTurn = g_turn; // load g_turn, store tTurn
//			if (tFlag != 0x02) // 2번 비트가 안켜졌으면 진행
//				break;
//			if (tTurn != 0) // load tTurn
//				break;
//		}
//
//		if (InterlockedExchange(&g_lock, 1) == 2)
//		{
//			// 2->1로 바뀐경우
//			//DebugBreak();
//		}
//
//		_result++;
//
//		if (InterlockedExchange(&g_lock, 0) == 2)
//		{
//			// 2->0로 바뀐경우
//			//DebugBreak();
//		}
//
//		g_FlagVal = (g_FlagVal & 0x02); // 첫번째 비트 끄기
//
//		cnt++;
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
//		g_FlagVal = (g_FlagVal | 0x02); // 0010 키기
//		g_turn = 1;
//
//		std::atomic_thread_fence(std::memory_order_seq_cst);
//		while (1)
//		{
//			tFlag = (g_FlagVal & 0x01); // 1번비트가 켜졌는가?
//			tTurn = g_turn;
//			if (tFlag != 0x01) // 1번비트가 안켜졌으면 진행
//				break;
//			if (tTurn != 1)
//				break;
//		}
//
//		if (InterlockedExchange(&g_lock, 2) == 1)
//		{
//			// 1->2로 바뀐 경우
//			//DebugBreak();
//		}
//
//		_result++;
//
//		if (InterlockedExchange(&g_lock, 0) == 1)
//		{
//			// 1->0로 바뀐경우
//			//DebugBreak();
//		}
//
//		g_FlagVal = (g_FlagVal & 0x01); // 두번째 비트 끄기
//
//		cnt++;
//		if (cnt == (int)lpThreadParameter)
//			break;
//	}
//
//	return 0;
//}

// 둘 다 임계영역에 있는 모습을 확인할 수 있는 버전
//UINT Thread1(LPVOID lpThreadParameter)
//{
//	int cnt = 0;
//
//	while (1)
//	{
//		g_Flag[0] = true; // store
//		g_turn = 1; //store
//
//		// Load, Store 순서가 바뀌는 조건
//		// Load가 Store 뒤에 있고, 둘이 연관 없는 데이터 ->
//		while (1)
//		{
//			int tFlag = g_Flag[1]; // load, store
//			LONG tTurn = g_turn; // load, store
//			if (tFlag == false) // load
//				break;
//			if (tTurn != 1) // load
//				break;
//		}
//
//		if (InterlockedExchange(&g_lock, 1) == 2)
//		{
//			// 2->1로 바뀐경우
//			DebugBreak();
//		}
//
//		_result++;
//
//		g_Flag[0] = false;
//		g_turn = 0;
//		if (InterlockedExchange(&g_lock, 0) == 2)
//		{
//			// 2->0로 바뀐경우
//			DebugBreak();
//		}
//
//		cnt++;
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
//		g_Flag[1] = true;
//		g_turn = 2;
//
//		while (1)
//		{
//			int tFlag = g_Flag[0];
//			LONG tTurn = g_turn;
//			if (tFlag == false)
//				break;
//			if (tTurn != 2)
//				break;
//		}
//
//		if (InterlockedExchange(&g_lock, 2) == 1)
//		{
//			// 1->2로 바뀐 경우
//			DebugBreak();
//		}
//
//		_result++;
//
//		g_Flag[1] = false;
//		g_turn = 0;
//
//		if (InterlockedExchange(&g_lock, 0) == 1)
//		{
//			// 1->0로 바뀐경우
//			DebugBreak();
//		}
//
//		cnt++;
//		if (cnt == (int)lpThreadParameter)
//			break;
//	}
//
//	return 0;
//}

// 임시변수로 확인용
//UINT Thread1(LPVOID lpThreadParameter)
//{
//	int cnt = 0;
//
//	while (1)
//	{
//		int tFlag;
//		int tTurn;
//
//		g_Flag[0] = true;
//		g_turn = 0;
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
//		if (g_Flag[1] == true && (g_turn != 1 || g_Flag[0] == false))
//		{
//			DebugBreak();
//		}
//
//		_result++;
//		cnt++;
//
//		g_Flag[0] = false; 
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
//		int tTurn;
//
//		g_Flag[1] = true;
//		g_turn = 1;
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
//		if (g_Flag[0] == true && (g_turn != 0 || g_Flag[1] == false))
//		{
//			DebugBreak();
//		}
//
//		_result++;
//		cnt++;
//
//		g_Flag[1] = false;
//
//		if (cnt == (int)lpThreadParameter)
//			break;
//	}
//
//	return 0;
//}
