#include "Includes.h"
#include "NetServer.h"

unsigned int WINAPI CNetServer::TPSThread(LPVOID arg)
{
	CNetServer* thisPtr = (CNetServer*)arg;
	while (1)
	{
		thisPtr->ResetTPS();
	}
}

void CNetServer::ResetTPS()
{
	printf("-----------------------------------------------------------------\n");
	printf("Session Count : %d\n", _iSessionCount);
	printf("AcceptTPS : %d\n", _iAcceptTPS);
	printf("RecvMessageTPS : %d\n", _iRecvMessageTPS);
	printf("SendMessageTPS : %d\n", _iSendMessageTPS);
	printf("\n");

	_iAcceptTPS = 0;
	_iRecvMessageTPS = 0;
	_iSendMessageTPS = 0;

	WaitForSingleObject(_hTPSUpdateEvent, 1000);
}