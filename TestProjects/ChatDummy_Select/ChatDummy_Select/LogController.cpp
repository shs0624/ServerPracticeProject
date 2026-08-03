#pragma once
#include "Includes.h"
#include "CommonProtocol.h"
#include "LogController.h"

LogController LogController::_LogController;

void LogController::LOG_SEND(INT64 accountNo, WORD packetType)
{
	DWORD idx = InterlockedIncrement(&_iLogCount) % dfLOG_MAX;

	_logArr[idx].type = SEND;
	_logArr[idx].packetType = (en_PACKET_TYPE)packetType;
	_logArr[idx].AccountNo = accountNo;
}

void LogController::LOG_RECV(INT64 accountNo, INT64 recvNo, WORD packetType)
{
	DWORD idx = InterlockedIncrement(&_iLogCount) % dfLOG_MAX;

	_logArr[idx].type = RECV;
	_logArr[idx].packetType = (en_PACKET_TYPE)packetType;
	_logArr[idx].recvNo = recvNo;
	_logArr[idx].AccountNo = accountNo;
}

void LogController::Init(SOCKADDR_IN serverAddr, int iClientCount, bool bTestTimeout, bool bTestFlood)
{
	_serverAddr = serverAddr;

	_iLogCount = 0;
	_iSessionCount = iClientCount;
	if (bTestTimeout)
	{
		_iTimeoutTestSessionCount = (_iSessionCount / 4) * dfTIMEOUTTEST_RATIO;
	}

	if (bTestFlood)
	{
		_iMessageFloodTestSessionCount = (_iSessionCount / 4) * dfFLOODTEST_RATIO;
	}

	_dwConnectWaitCount = 0;
	_dwLoginWaitCount = 0;
	_dwDisconnectFromServerCount = 0;
	_dwResponseFailCount = 0;
	_dwMessageNotRecvCount = 0;
	_dwLoginResNotRecvCount = 0;
	_dwNeedTimeoutSessionCount = 0;
	_dwNeedTimeoutUserCount = 0;

	_dwNormalDisconnectCount = 0;
	_dwIntendedDisconnectSessionCount = 0;

	_dwLoginSendCount = 0;
	_dwMoveSendCount = 0;
	_dwChatSendCount = 0;

	_hLogUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	_htpsThreadHandle = (HANDLE)_beginthreadex(NULL, 0, LogingThread, this, 0, &_tpsThreadID);
	if (_htpsThreadHandle == NULL)
		DebugBreak();
}

unsigned int WINAPI LogController::LogingThread(LPVOID arg)
{
	LogController* thisPtr = (LogController*)arg;

	while (1)
	{
		thisPtr->PrintLog();

		// TPS ÃÊ±âÈ­
		thisPtr->_dwRecvMessageTPS = 0;
		thisPtr->_dwSendMessageTPS = 0;

		WaitForSingleObject(thisPtr->_hLogUpdateEvent, 1000);
	}

	return 0;
}

void LogController::PrintLog()
{
	//system("cls");
	char ipBuf[INET_ADDRSTRLEN] = { 0 };
	short port = ntohs(_serverAddr.sin_port);
	inet_ntop(AF_INET, &(_serverAddr.sin_addr), ipBuf, INET_ADDRSTRLEN);

	printf("S : Echo PLAY | Q : Quit\n");
	//wprintf(L"C : Reconnect STOP\n\n");

	printf("==============================================================================\n");
	printf("Server IP:%-s| Server Port: %-3d\n",
		ipBuf, port);
	printf("==============================================================================\n");
	printf("Client:%-5d| Thread: %-2d\nTimeOutTest_User : %4d| TimeOutTest_Session : %4d | MessageFlood_Session : %4d\n",
		_iSessionCount, 4, _iTimeoutTestSessionCount * 4, _iTimeoutTestSessionCount * 4, _iMessageFloodTestSessionCount * 4);
	printf("==============================================================================\n");

	//printf("%-25s%5d\n", "Thread Loop :", 0);
	//printf("%-25s%5ls\n", "Max Latency :", "0 ms");

	printf("%-25s%5d\n", "Connect Try :", _dwConnectTry);
	printf("%-25s%5d\n", "Connect Success :", _dwConnectSuccess);
	printf("==============================================================================\n");
	printf("%-25s%5d\n", "Login	 Send :", _dwLoginSendCount);
	printf("%-25s%5d\n", "Move	 Send :", _dwMoveSendCount);
	printf("%-25s%5d\n", "Chat	 Send :", _dwChatSendCount);
	printf("==============================================================================\n");
	printf("%-25s%5d\n", "Error - Connect Fail :", _dwConnectFail);
	printf("%-25s%5d\n", "Error - Disconnect from Server :", _dwDisconnectFromServerCount);
	printf("%-25s%5d\n", "Error - Timeout - Not Recv :", _dwMessageNotRecvCount);
	printf("%-25s%5d\n", "Error - Timeout - Not Recv Login Response :", _dwLoginResNotRecvCount);
	printf("%-25s%5d\n", "Error - Need Timeout - Session :", _dwNeedTimeoutSessionCount);
	printf("%-25s%5d\n", "Error - Need Timeout - User :", _dwNeedTimeoutUserCount);
	printf("==============================================================================\n");
	printf("%-25s%5d\n", "Success - Normal Disconnect :", _dwNormalDisconnectCount);
	printf("%-25s%5d\n", "Success - ErorrCheck Disconnect :", _dwIntendedDisconnectSessionCount);
	printf("==============================================================================\n");
	printf("\n%-25s%5d\n", "PacketPool Use :", 0);
	printf("%-25s%5d\n", "SendPacket TPS :", _dwSendMessageTPS);
	printf("%-25s%5d\n", "RecvPacket TPS :", _dwRecvMessageTPS);
}
