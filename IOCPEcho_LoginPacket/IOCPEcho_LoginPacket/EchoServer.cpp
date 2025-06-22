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

void EchoServer::OnAccept(ULONGLONG SessionID)
{
	RefCountPointer<CPacket> cPacket = RefCountPointer<CPacket>::MakeSharedPtr();
	(*cPacket)->Initialize(PROTOCOL_MAX_SIZE + 1, sizeof(st_NetHeader));

	LONGLONG login = 0x7fffffffffffffff;
	*(*cPacket) << (LONGLONG)login;

	SendPacket(SessionID, cPacket);
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