#include "CSerializationBuffer.h"
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
			ProfileDataOutText("ProfileData.txt");
		}

	}
}

void EchoServer::OnAccept(ULONGLONG SessionID)
{
	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	(*cPacket)->Initialize(PROTOCOL_MAX_SIZE + 1, sizeof(st_NetHeader));

	__int64 login = 0x7fffffffffffffff;
	*(*cPacket) << login;

	SendLoginPacket(SessionID, cPacket);
}

void EchoServer::OnRelease(ULONGLONG SessionID)
{

}

void EchoServer::OnRecv(ULONGLONG SessionID, CPacket* cpacket)
{
	//Profiler pro(L"OnRecv");
	/*char temp[PROTOCOL_MAX_SIZE + 1];

	int iSize = cpacket->GetDataSize();
	cpacket->GetData(temp, iSize);

	CPacket sendCPacket;
	sendCPacket.PutData(temp, iSize);

	SendPacket(SessionID, &sendCPacket);*/
}

void EchoServer::OnError(int errorcode, WCHAR* message)
{

}