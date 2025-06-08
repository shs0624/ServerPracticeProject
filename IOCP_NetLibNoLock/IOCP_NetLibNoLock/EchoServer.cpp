#include "EchoServer.h"
#include <conio.h>

int main()
{
	EchoServer* _echoServer = new EchoServer(INADDR_ANY, 6000, true, 500);

	char ch;
	while (1)
	{
		// ÄÁÆ®·Ñ?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			_echoServer->QuitServer();
			break;
		}
	}
}

void EchoServer::OnAccept()
{

}

void EchoServer::OnRelease(ULONG SessionID)
{

}

void EchoServer::OnRecv(ULONG SessionID, CPacket* cpacket)
{
	char temp[PROTOCOL_MAX_SIZE + 1];

	int iSize = cpacket->GetDataSize();
	cpacket->GetData(temp, iSize);

	CPacket sendCPacket;
	sendCPacket.PutData(temp, iSize);

	SendPacket(SessionID, &sendCPacket);
}

void EchoServer::OnError(int errorcode, WCHAR* message)
{

}