#pragma once
#include <Windows.h>
#include <process.h>
#include <iostream>
#define dfLOG_MAX 10000

struct stChatLog
{
	LONG _lQueueSize;
	LONG _lInsertTPS;
};

class LogController
{
public:
	static LogController* GetInstance()
	{
		static LogController logC;
		return &logC;
	}

	// 외부의 스레드 저장소를
	void RegisterLogStruct(stChatLog* pLog)
	{
		int idx = InterlockedIncrement(&_dwLogArrIdx);
		_LogStructArr[idx] = pLog;
	}

	// 이걸로 로그 구조체를 할당해줌(TLS). 받는 스레드는 이 주소를 저장하고 사용
	stChatLog* AllocLogStruct()
	{
		stChatLog* ptr = (stChatLog*)TlsGetValue(_dwTlsIdx);
		if (ptr == NULL)
		{
			ptr = (stChatLog*)malloc(sizeof(stChatLog));
			int idx = InterlockedIncrement(&_dwLogArrIdx);
			_LogStructArr[idx] = ptr;
		}


		return ptr;
	}

	// 내가 할당한 주소를 쭉 훑으며 내 지역변수를 변경
	void ReadLog()
	{
		memset(&_stPrintLog, 0, sizeof(_stPrintLog));
		for (int i = 1; i <= _dwLogArrIdx; i++)
		{
			_stPrintLog._lQueueSize += _LogStructArr[i]->_lQueueSize;
			_stPrintLog._lInsertTPS += _LogStructArr[i]->_lInsertTPS;
		}
	}

	// ReadLog가 선행된 후 내 지역변수 값을 출력
	void PrintLog()
	{
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Queue Size :", _stPrintLog._lQueueSize);
		printf("%-25s%5d\n", "Insert TPS :", _stPrintLog._lInsertTPS);
		printf("==============================================================================\n\n\n");
	}

	static LogController _LogController;
private:
	LogController()
	{
		_dwTlsIdx = TlsAlloc();

		_hLogUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		_htpsThreadHandle = (HANDLE)_beginthreadex(NULL, 0, LogingThread, this, 0, &_tpsThreadID);
		if (_htpsThreadHandle == NULL)
			DebugBreak();
	};

	~LogController() {};

	void ResetTPS()
	{
		for (int i = 1; i <= _dwLogArrIdx; i++)
		{
			_LogStructArr[i]->_lInsertTPS = 0;
		}
	}

	static unsigned int WINAPI LogingThread(LPVOID arg)
	{
		LogController* thisPtr = (LogController*)arg;

		while (1)
		{
			thisPtr->ReadLog();

			thisPtr->ResetTPS();

			thisPtr->PrintLog();

			WaitForSingleObject(thisPtr->_hLogUpdateEvent, 1000);
		}

		return 0;
	}

	stChatLog _stPrintLog;

	// 몇 번 TLS 주소에 구조체가 저장되어 있는가
	DWORD _dwTlsIdx;
	DWORD _dwLogArrIdx;
	stChatLog* _LogStructArr[100];

	HANDLE _hLogUpdateEvent;
	HANDLE _htpsThreadHandle;
	unsigned int _tpsThreadID;

	DWORD _iLogCount;
};