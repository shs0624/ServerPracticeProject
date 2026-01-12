#pragma once
#include "Includes.h"

#define dfLOG_MAX 10000

struct stChatLog
{
	LONG _dwChatRecvMessageTPS;
	LONG _dwClientRecvMessageTPS;
	LONG _dwClientSendMessageTPS;

	LONG _dwChatAcceptTotal;
	LONG _dwClientAcceptTotal;

	LONG _dwServerCount;
	LONG _dwClientCount;

	LONG _dwPacketPoolUse;
	LONG _dwDuplicatedLoginTotal;
	LONG _dwDecodeDisconnectTotal;

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
			_stPrintLog._dwChatRecvMessageTPS += _LogStructArr[i]->_dwChatRecvMessageTPS;
			_stPrintLog._dwClientRecvMessageTPS += _LogStructArr[i]->_dwClientRecvMessageTPS;
			_stPrintLog._dwClientSendMessageTPS += _LogStructArr[i]->_dwClientSendMessageTPS;
			_stPrintLog._dwChatAcceptTotal += _LogStructArr[i]->_dwChatAcceptTotal;
			_stPrintLog._dwClientAcceptTotal += _LogStructArr[i]->_dwClientAcceptTotal;
			_stPrintLog._dwServerCount += _LogStructArr[i]->_dwServerCount;
			_stPrintLog._dwClientCount += _LogStructArr[i]->_dwClientCount;
			_stPrintLog._dwPacketPoolUse += _LogStructArr[i]->_dwPacketPoolUse;
			_stPrintLog._dwDuplicatedLoginTotal += _LogStructArr[i]->_dwDuplicatedLoginTotal;
			_stPrintLog._dwDecodeDisconnectTotal += _LogStructArr[i]->_dwDecodeDisconnectTotal;
			_stPrintLog._dwTimeoutSessionTotal += _LogStructArr[i]->_dwTimeoutSessionTotal;
			_stPrintLog._dwTimeoutUserTotal += _LogStructArr[i]->_dwTimeoutUserTotal;
		}
	}

	// ReadLog가 선행된 후 내 지역변수 값을 출력
	void PrintLog()
	{
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Server Count :", _stPrintLog._dwServerCount);
		printf("%-25s%5d\n", "Client Count :", _stPrintLog._dwClientCount);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Server Accept Total : ", _stPrintLog._dwChatAcceptTotal);
		printf("%-25s%5d\n", "Client Accept Total : ", _stPrintLog._dwClientAcceptTotal);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Server Recv TPS  :", _stPrintLog._dwChatRecvMessageTPS);
		printf("%-25s%5d\n", "Client Recv TPS :", _stPrintLog._dwClientRecvMessageTPS);
		printf("%-25s%5d\n", "Client Send TPS :", _stPrintLog._dwClientSendMessageTPS);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Duplicated Login Total :", _stPrintLog._dwDuplicatedLoginTotal);
		printf("%-25s%5d\n", "Decode Disconnect Total :", _stPrintLog._dwDecodeDisconnectTotal);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Timeout_Session :", _stPrintLog._dwTimeoutSessionTotal);
		printf("%-25s%5d\n", "Timeout_User   :", _stPrintLog._dwTimeoutUserTotal);
		printf("==============================================================================\n\n\n");
		printf("%-25s%5d\n", "PacketPool Use :", _stPrintLog._dwPacketPoolUse);
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
			_LogStructArr[i]->_dwChatRecvMessageTPS = 0;
			_LogStructArr[i]->_dwClientRecvMessageTPS = 0;
			_LogStructArr[i]->_dwClientSendMessageTPS = 0;
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