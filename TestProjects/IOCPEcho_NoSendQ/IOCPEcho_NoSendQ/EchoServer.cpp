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
			ProfileDataOutText("ProfileData_IdxStack_myStack.txt");
		}

	}
}

void EchoServer::OnAccept()
{

}

void EchoServer::OnRelease(ULONGLONG SessionID)
{

}

void EchoServer::OnRecv(ULONGLONG SessionID, CPacket* cpacket)
{
	//Profiler pro(L"OnRecv");
	//char temp[PROTOCOL_MAX_SIZE + 1];

	int iSize = cpacket->GetDataSize();
	//cpacket->GetData(temp, iSize);

	//CPacket sendCPacket(PROTOCOL_MAX_SIZE + 1);
	//sendCPacket.PutData(temp, iSize);

	//SendPacket(SessionID, &sendCPacket);
	SendPacket(SessionID, cpacket);
}

void EchoServer::OnError(int errorcode, WCHAR* message)
{

}