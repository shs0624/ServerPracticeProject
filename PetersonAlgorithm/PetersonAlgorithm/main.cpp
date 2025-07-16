#include <Windows.h>
#include <process.h>
#include <iostream>

//#define BREAK
#define LOG
//#define SETAFF

enum InOut
{
	CSIN,
	CSOUT
};

struct st_LOG
{
	InOut inOutInfo;
	int ThreadNum;
	int tFlag;
	int tTurn;
	int tResult;
};

int g_Flag[2];
int g_cs[2];
int g_FlagVal = 0;
LONG g_turn;
LONG g_lock;

int _result = 0;
DWORD _criticalCount;

st_LOG _logArr[100000];

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

	threadArr[0] = (HANDLE)_beginthreadex(NULL, 0, Thread1, (LPVOID)30000000, 0, &dwThread1Id);
	threadArr[1] = (HANDLE)_beginthreadex(NULL, 0, Thread2, (LPVOID)30000000, 0, &dwThread2Id);

#ifdef SETAFF
	SetThreadPriority(threadArr[0], THREAD_PRIORITY_HIGHEST);
	SetThreadPriority(threadArr[1], THREAD_PRIORITY_LOWEST);

	SetThreadAffinityMask(threadArr[0], 1 << 0);
	SetThreadAffinityMask(threadArr[1], 1 << 0);
#endif

	WaitForMultipleObjects(2, threadArr, true, INFINITE);

	for (int i = 0; i < _criticalCount; i++)
	{
		if (_logArr[i].inOutInfo == CSIN)
			wprintf(L"Enter Critical Point Thread%d | tFlag : %d # tTurn : %d # Result : %d\n",
				_logArr[i].ThreadNum, _logArr[i].tFlag, _logArr[i].tTurn, _logArr[i].tResult);
		else
			wprintf(L"Leave Critical Point Thread%d | tFlag : %d # tTurn : %d # Result : %d\n",
				_logArr[i].ThreadNum, _logArr[i].tFlag, _logArr[i].tTurn, _logArr[i].tResult);
	}

	printf("result : %d\n", _result);

	return 0;
}

UINT Thread1(LPVOID lpThreadParameter)
{
	int loopCount = (int)lpThreadParameter;
	for (int i = 0; i < loopCount; i++)
	{
		LONG tFlag;
		LONG tTurn;
		DWORD tTemp;

		g_Flag[0] = true; // store Flag[0]
		g_turn = 0; //store g_turn

		//InterlockedIncrement(&tTemp);

		while (1)
		{
			tFlag = g_Flag[1]; // load Flag[1], store tFlag
			tTurn = g_turn; // load g_turn, store tTurn

			if (tFlag == false) // load tFlag  
				break;
			if (tTurn != 0) // load tTurn
				break;
		}

		if (InterlockedExchange(&g_lock, 1) == 2)
		{
			// 2->1로 바뀐경우
#ifdef BREAK
			DebugBreak();
#endif
#ifdef LOG
			int idx = InterlockedIncrement(&_criticalCount) - 1;
			_logArr[idx].inOutInfo = CSIN;
			_logArr[idx].tFlag = tFlag;
			_logArr[idx].tTurn = tTurn;
			_logArr[idx].ThreadNum = 1;
			_logArr[idx].tResult = _result;
#endif
		}

		_result++;

		if (InterlockedExchange(&g_lock, 0) == 2)
		{
			// 2->0로 바뀐경우
#ifdef BREAK
			DebugBreak();
#endif

#ifdef LOG
			/*int idx = InterlockedIncrement(&_criticalCount) - 1;
			_logArr[idx].inOutInfo = CSOUT;
			_logArr[idx].tFlag = tFlag;
			_logArr[idx].tTurn = tTurn;
			_logArr[idx].ThreadNum = 1;
			_logArr[idx].tResult = _result;*/
#endif
		}
		g_Flag[0] = false;
	}

	return 0;
}

UINT Thread2(LPVOID lpThreadParameter)
{
	int loopCount = (int)lpThreadParameter;
	for (int i = 0; i < loopCount; i++)
	{
		LONG tFlag;
		LONG tTurn;
		DWORD tTemp;

		g_Flag[1] = true; // store Flag[0]
		g_turn = 1; //store g_turn

		//InterlockedIncrement(&tTemp);

		while (1)
		{
			tFlag = g_Flag[0]; // load Flag[1], store tFlag
			tTurn = g_turn; // load g_turn, store tTurn

			if (tFlag == false) // load tFlag  
				break;
			if (tTurn != 1) // load tTurn
				break;
		}

		if (InterlockedExchange(&g_lock, 2) == 1)
		{
			// 2->1로 바뀐경우
#ifdef BREAK
			DebugBreak();
#endif
#ifdef LOG
			int idx = InterlockedIncrement(&_criticalCount) - 1;
			_logArr[idx].inOutInfo = CSIN;
			_logArr[idx].tFlag = tFlag;
			_logArr[idx].tTurn = tTurn;
			_logArr[idx].ThreadNum = 2;
			_logArr[idx].tResult = _result;
#endif
		}

		_result++;

		if (InterlockedExchange(&g_lock, 0) == 1)
		{
			// 2->0로 바뀐경우
#ifdef BREAK
			DebugBreak();
#endif
#ifdef LOG
			/*int idx = InterlockedIncrement(&_criticalCount) - 1;
			_logArr[idx].inOutInfo = CSOUT;
			_logArr[idx].tFlag = tFlag;
			_logArr[idx].tTurn = tTurn;
			_logArr[idx].ThreadNum = 2;
			_logArr[idx].tResult = _result;*/
#endif
		}
		g_Flag[1] = false;
	}

	return 0;
}