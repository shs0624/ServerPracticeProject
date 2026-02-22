#pragma once
#include "PDHMonitor.h"
#include "Includes.h"
#include "CPUUsage.h"
#include "LanClient.h"
#include "MonitorProtocol.h"
#include "MonitorClient.h"
#define dfLOG_MAX 10000
#define MONITORING_ON

struct stChatLog
{
	DWORD _dwRecvMessageTPS;
	DWORD _dwSendMessageTPS;

	DWORD _dwAcceptTotal;
	DWORD _dwAcceptTPS;

	DWORD _dwSessionCount;
	DWORD _dwAuthUserCount;
	DWORD _dwGameUserCount;

	DWORD _dwPacketPoolUse;
	DWORD _dwPlayerPoolUse;

	DWORD _dwAuthFPS;
	DWORD _dwGameFPS;

	DWORD _dwLoginMessageTPS;
	DWORD _dwEchoMessageTPS;
	DWORD _dwLoginQueueSize;
	DWORD _dwEchoQueueSize;

	DWORD _dwDuplicatedLoginTotal;
	DWORD _dwDecodeDisconnectTotal;
	DWORD _dwNotCorrectAccountNumTotal;
	DWORD _dwRedisCertificationFailTotal;
	DWORD _dwDisconnectExcessiveMessageTotal;

	DWORD _dwDisconnectInvalidAccountNum;
	DWORD _dwDisconnectLenOverMax;
	DWORD _dwDisconnectMaxSession;

	DWORD _dwTimeoutSessionTotal;
	DWORD _dwTimeoutUserTotal;
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
			_stPrintLog._dwAuthUserCount += _LogStructArr[i]->_dwAuthUserCount;
			_stPrintLog._dwGameUserCount += _LogStructArr[i]->_dwGameUserCount;
			_stPrintLog._dwAcceptTotal += _LogStructArr[i]->_dwAcceptTotal;
			_stPrintLog._dwAcceptTPS += _LogStructArr[i]->_dwAcceptTPS;
			_stPrintLog._dwRecvMessageTPS += _LogStructArr[i]->_dwRecvMessageTPS;
			_stPrintLog._dwSendMessageTPS += _LogStructArr[i]->_dwSendMessageTPS;
			_stPrintLog._dwLoginMessageTPS += _LogStructArr[i]->_dwLoginMessageTPS;
			_stPrintLog._dwEchoMessageTPS += _LogStructArr[i]->_dwEchoMessageTPS;
			_stPrintLog._dwAuthFPS += _LogStructArr[i]->_dwAuthFPS;
			_stPrintLog._dwGameFPS += _LogStructArr[i]->_dwGameFPS;
			_stPrintLog._dwDuplicatedLoginTotal += _LogStructArr[i]->_dwDuplicatedLoginTotal;
			_stPrintLog._dwDecodeDisconnectTotal += _LogStructArr[i]->_dwDecodeDisconnectTotal;
			_stPrintLog._dwNotCorrectAccountNumTotal += _LogStructArr[i]->_dwNotCorrectAccountNumTotal;
			_stPrintLog._dwRedisCertificationFailTotal += _LogStructArr[i]->_dwRedisCertificationFailTotal;
			_stPrintLog._dwDisconnectExcessiveMessageTotal += _LogStructArr[i]->_dwDisconnectExcessiveMessageTotal;
			_stPrintLog._dwDisconnectInvalidAccountNum += _LogStructArr[i]->_dwDisconnectInvalidAccountNum;
			_stPrintLog._dwDisconnectLenOverMax += _LogStructArr[i]->_dwDisconnectLenOverMax;
			_stPrintLog._dwDisconnectMaxSession += _LogStructArr[i]->_dwDisconnectMaxSession;
			_stPrintLog._dwTimeoutSessionTotal += _LogStructArr[i]->_dwTimeoutSessionTotal;
			_stPrintLog._dwTimeoutUserTotal += _LogStructArr[i]->_dwTimeoutUserTotal;
			_stPrintLog._dwPacketPoolUse += _LogStructArr[i]->_dwPacketPoolUse;
			_stPrintLog._dwPlayerPoolUse += _LogStructArr[i]->_dwPlayerPoolUse;

			_stPrintLog._dwSessionCount += (_LogStructArr[i]->_dwAuthUserCount + _LogStructArr[i]->_dwGameUserCount);
		}
	}

	// ReadLog가 선행된 후 내 지역변수 값을 출력
	void PrintLog()
	{
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Auth User Count :", _stPrintLog._dwAuthUserCount);
		printf("%-25s%5d\n", "Game User Count :", _stPrintLog._dwGameUserCount);
		printf("%-25s%5d\n", "Session Count :", _stPrintLog._dwSessionCount);
		printf("%-25s%5d\n", "Accept  Total :", _stPrintLog._dwAcceptTotal);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Accept TPS : ", _stPrintLog._dwAcceptTPS);
		printf("%-25s%5d\n", "RecvPacket TPS : ", _stPrintLog._dwRecvMessageTPS);
		printf("%-25s%5d\n", "SendPacket TPS : ", _stPrintLog._dwSendMessageTPS);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Contents - Login TPS :", _stPrintLog._dwLoginMessageTPS);
		printf("%-25s%5d\n", "Contents - Echo  TPS :", _stPrintLog._dwEchoMessageTPS);
		printf("%-25s%5d\n", "Contents - Auth  FPS :", _stPrintLog._dwAuthFPS);
		printf("%-25s%5d\n", "Contents - Game  FPS :", _stPrintLog._dwGameFPS);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Duplicated Login Total :", _stPrintLog._dwDuplicatedLoginTotal);
		printf("%-25s%5d\n", "Decode Disconnect Total :", _stPrintLog._dwDecodeDisconnectTotal);
		printf("%-25s%5d\n", "Not Correct AccountNum Total :", _stPrintLog._dwNotCorrectAccountNumTotal);
		printf("%-25s%5d\n", "Redis Certification Fail Total :", _stPrintLog._dwRedisCertificationFailTotal);
		printf("%-25s%5d\n", "Disconnect MaxSession Total :", _stPrintLog._dwDisconnectMaxSession);
		//printf("%-25s%5d\n", "Disconnect Excessive Message Total :", _stPrintLog._lDisconnectExcessiveMessageTotal);
		//printf("%-25s%5d\n", "Disconnect InvalidAccountNum Total :", _stPrintLog._lDisconnectInvalidAccountNum);
		//printf("%-25s%5d\n", "Disconnect LenOverMax Total :", _stPrintLog._lDisconnectLenOverMax);
		//printf("%-25s%5d\n", "Disconnect OutOfMoveRange Total :", _stPrintLog._lDisconnectOutOfMoveRange);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Timeout_Session :", _stPrintLog._dwTimeoutSessionTotal);
		printf("%-25s%5d\n", "Timeout_User   :", _stPrintLog._dwTimeoutUserTotal);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "PacketPool Use :", _stPrintLog._dwPacketPoolUse);
		printf("%-25s%5d\n", "UserPool Use   :", _stPrintLog._dwPlayerPoolUse);
		printf("==============================================================================\n\n\n");
	}

	static LogController _LogController;
private:
	LogController()
	{
		_dwTlsIdx = TlsAlloc();

#ifdef MONITORING_ON
		_pPDHMonitor = new PDHMonitor();
		_pCpuUsage = new CCpuUsage();

		_pMonitorClient = new MonitorClient();
		_pMonitorClient->InitMonitorClient();
#endif

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

			_LogStructArr[i]->_dwLoginMessageTPS = 0;
			_LogStructArr[i]->_dwEchoMessageTPS = 0;

			_LogStructArr[i]->_dwAuthFPS = 0;
			_LogStructArr[i]->_dwGameFPS = 0;

			_LogStructArr[i]->_dwRecvMessageTPS = 0;
			_LogStructArr[i]->_dwSendMessageTPS = 0;
		}
	}

#ifdef MONITORING_ON
	void SendChatMonitorPacket()
	{
		// 1초마다 갱신
		_pCpuUsage->UpdateCpuTime();
		_pPDHMonitor->QueryUpdate();

		// PDH로 서버 CPU, MEM 얻기
		int timeStamp = (int)time(NULL);
		int cpuUsage = _pCpuUsage->ProcessTotal();//_pPDHMonitor->GetCPUUsage();
		int memoryMB = _pPDHMonitor->GetPrivateMemory() / 1000000;

		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_GAME_SERVER_RUN, true, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_GAME_SERVER_CPU, cpuUsage, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_GAME_SERVER_MEM, memoryMB, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_GAME_SESSION, _stPrintLog._dwSessionCount, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER, _stPrintLog._dwGameUserCount, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER, _stPrintLog._dwAuthUserCount, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_GAME_PACKET_POOL, _stPrintLog._dwPacketPoolUse, timeStamp);
		//_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL, _stPrintLog., timeStamp);
		
	}
#endif

	static unsigned int WINAPI LogingThread(LPVOID arg)
	{
		LogController* thisPtr = (LogController*)arg;

		while (1)
		{
			thisPtr->ReadLog();

#ifdef MONITORING_ON
			// 채팅서버 로그를 보내자.
			thisPtr->SendChatMonitorPacket();
#endif

			thisPtr->ResetTPS();

			thisPtr->PrintLog();

			WaitForSingleObject(thisPtr->_hLogUpdateEvent, 1000);
		}

		return 0;
	}

#ifdef MONITORING_ON
	CCpuUsage* _pCpuUsage;
	PDHMonitor* _pPDHMonitor;
	MonitorClient* _pMonitorClient;
#endif

	stChatLog _stPrintLog;

	// 몇 번 TLS 주소에 구조체가 저장되어 있는가
	DWORD _dwTlsIdx;
	DWORD _dwLogArrIdx;
	stChatLog* _LogStructArr[500];

	HANDLE _hLogUpdateEvent;
	HANDLE _htpsThreadHandle;
	unsigned int _tpsThreadID;

	DWORD _iLogCount;
};