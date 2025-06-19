#include "EchoServer.h"
#include "ProcademyProfiler.h"
#include <conio.h>

int main()
{
	EchoServer* _echoServer = new EchoServer(INADDR_ANY, SERVERPORT, true, 500);

	char ch;
	while (1)
	{
		// ÄÁÆ®·Ñ?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			_echoServer->QuitServer();
			//break;
		}
		if (ch == 'P' || ch == 'p')
		{
			ProfileDataOutText("ProfileData_CPacketRefCount_NewDelete.txt");
		}

	}
}

void EchoServer::OnAccept()
{

}

void EchoServer::OnRelease(ULONGLONG SessionID)
{

}

void EchoServer::OnRecv(ULONGLONG SessionID, RefCountPointer<CPacket> cpacket)
{
	SendPacket(SessionID, cpacket);
}

void EchoServer::OnError(int errorcode, WCHAR* message)
{

}