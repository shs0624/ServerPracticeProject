#pragma once
#include "Includes.h"
#include "CommonProtocol.h"
#include "LogManager.h"

LogController LogController::_LogController;

void LogController::Init()
{
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
		thisPtr->_dwAcceptTPS = 0;
		thisPtr->_dwUpdateTPS = 0;
		thisPtr->_dwUpdateThreadSleepTime = 0;

		thisPtr->_dwChatMessageTPS = 0;
		thisPtr->_dwLoginMessageTPS = 0;
		thisPtr->_dwMoveMessageTPS = 0;

		thisPtr->_dwRecvMessageTPS = 0;
		thisPtr->_dwSendMessageTPS = 0;

		WaitForSingleObject(thisPtr->_hLogUpdateEvent, 1000);
	}

	return 0;
}

void LogController::PrintLog()
{
	printf("==============================================================================\n");
	printf("%-25s%5d\n", "User Count :", _dwUserCount);
	printf("%-25s%5d\n", "Session Count :", _dwSessionCount);
	printf("%-25s%5d\n", "Accept  Total :", _dwAcceptTotal);
	printf("==============================================================================\n");
	printf("%-25s%5d\n", "Update TPS :", _dwUpdateTPS);
	printf("%-25s%5d\n", "Update Q Size :", _dwUpdateQSize);
	printf("%-25s%5d\n", "Update Thread SleepTime :", _dwUpdateThreadSleepTime);
	printf("%-25s%5d\n", "Accept TPS : ", _dwAcceptTPS);
	printf("%-25s%5d\n", "RecvPacket TPS : ", _dwRecvMessageTPS);
	printf("%-25s%5d\n", "SendPacket TPS : ", _dwSendMessageTPS);
	printf("==============================================================================\n");
	printf("%-25s%5d\n", "Contents - Login TPS :", _dwLoginMessageTPS);
	printf("%-25s%5d\n", "Contents - Move  TPS :", _dwMoveMessageTPS);
	printf("%-25s%5d\n", "Contents - Chat  TPS :", _dwChatMessageTPS);
	printf("==============================================================================\n");
	printf("%-25s%5d\n", "Timeout_Session :", _dwTimeoutSessionTotal);
	printf("%-25s%5d\n", "Timeout_User   :", _dwTimeoutUserTotal);
	printf("==============================================================================\n\n\n");
	printf("%-25s%5d\n", "PacketPool Use :", _dwPacketPoolUse);
	printf("%-25s%5d\n", "UserPool Use   :", _dwPlayerPoolUse);
	printf("==============================================================================\n\n\n");
}
