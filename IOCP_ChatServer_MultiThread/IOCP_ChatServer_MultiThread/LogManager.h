#pragma once
#include "Includes.h"
#define dfLOG_MAX 10000

struct stChatLog
{
	LONG _dwRecvMessageTPS;
	LONG _dwSendMessageTPS;

	LONG _dwAcceptTotal;
	LONG _dwAcceptTPS;
	LONG _dwUpdateQSize;
	LONG _dwUpdateThreadSleepTime;

	LONG _dwSessionCount;
	LONG _dwUserCount;

	LONG _dwPacketPoolUse;
	LONG _dwPlayerPoolUse;

	LONG _dwMoveMessageTPS;
	LONG _dwChatMessageTPS;
	LONG _dwLoginMessageTPS;

	LONG _dwTimeoutSessionTotal;
	LONG _dwTimeoutUserTotal;
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

	// 내가 할당한 주소를 쭉 훑으며 내 지역변수를 변경
	void ReadLog()
	{
		memset(&_stPrintLog, 0, sizeof(_stPrintLog));
		for (int i = 1; i <= _dwLogArrIdx; i++)
		{
			_stPrintLog._dwUserCount += _LogStructArr[i]->_dwUserCount;
			_stPrintLog._dwSessionCount += _LogStructArr[i]->_dwSessionCount;
			_stPrintLog._dwAcceptTotal += _LogStructArr[i]->_dwAcceptTotal;
			_stPrintLog._dwUpdateQSize += _LogStructArr[i]->_dwUpdateQSize;
			_stPrintLog._dwUpdateThreadSleepTime += _LogStructArr[i]->_dwUpdateThreadSleepTime;
			_stPrintLog._dwRecvMessageTPS += _LogStructArr[i]->_dwRecvMessageTPS;
			_stPrintLog._dwSendMessageTPS += _LogStructArr[i]->_dwSendMessageTPS;
			_stPrintLog._dwLoginMessageTPS += _LogStructArr[i]->_dwLoginMessageTPS;
			_stPrintLog._dwMoveMessageTPS += _LogStructArr[i]->_dwMoveMessageTPS;
			_stPrintLog._dwChatMessageTPS += _LogStructArr[i]->_dwChatMessageTPS;
			_stPrintLog._dwTimeoutSessionTotal += _LogStructArr[i]->_dwTimeoutSessionTotal;
			_stPrintLog._dwTimeoutUserTotal += _LogStructArr[i]->_dwTimeoutUserTotal;
			_stPrintLog._dwPacketPoolUse += _LogStructArr[i]->_dwPacketPoolUse;
			_stPrintLog._dwPlayerPoolUse += _LogStructArr[i]->_dwPlayerPoolUse;
		}
	}

	// ReadLog가 선행된 후 내 지역변수 값을 출력
	void PrintLog()
	{
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "User Count :", _stPrintLog._dwUserCount);
		printf("%-25s%5d\n", "Session Count :", _stPrintLog._dwSessionCount);
		printf("%-25s%5d\n", "Accept  Total :", _stPrintLog._dwAcceptTotal);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Update Q Size :", _stPrintLog._dwUpdateQSize);
		printf("%-25s%5d\n", "Update Thread SleepTime :", _stPrintLog._dwUpdateThreadSleepTime);
		printf("%-25s%5d\n", "Accept TPS : ", _stPrintLog._dwAcceptTPS);
		printf("%-25s%5d\n", "RecvPacket TPS : ", _stPrintLog._dwRecvMessageTPS);
		printf("%-25s%5d\n", "SendPacket TPS : ", _stPrintLog._dwSendMessageTPS);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Contents - Login TPS :", _stPrintLog._dwLoginMessageTPS);
		printf("%-25s%5d\n", "Contents - Move  TPS :", _stPrintLog._dwMoveMessageTPS);
		printf("%-25s%5d\n", "Contents - Chat  TPS :", _stPrintLog._dwChatMessageTPS);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Timeout_Session :", _stPrintLog._dwTimeoutSessionTotal);
		printf("%-25s%5d\n", "Timeout_User   :", _stPrintLog._dwTimeoutUserTotal);
		printf("==============================================================================\n\n\n");
		printf("%-25s%5d\n", "PacketPool Use :", _stPrintLog._dwPacketPoolUse);
		printf("%-25s%5d\n", "UserPool Use   :", _stPrintLog._dwPlayerPoolUse);
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
			_LogStructArr[i]->_dwAcceptTPS = 0;
			_LogStructArr[i]->_dwUpdateThreadSleepTime = 0;

			_LogStructArr[i]->_dwChatMessageTPS = 0;
			_LogStructArr[i]->_dwLoginMessageTPS = 0;
			_LogStructArr[i]->_dwMoveMessageTPS = 0;

			_LogStructArr[i]->_dwRecvMessageTPS = 0;
			_LogStructArr[i]->_dwSendMessageTPS = 0;
		}
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