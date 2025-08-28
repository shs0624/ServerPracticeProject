#include "CLanServer.h"

unsigned int WINAPI CLanServer::TPSThread(LPVOID arg)
{
	CLanServer* thisPtr = (CLanServer*)arg;
	while (1)
	{
		thisPtr->ResetTPS();
	}
}

void CLanServer::ResetTPS()
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