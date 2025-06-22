#pragma once
#include "CLanServer.h"
#define SERVERPORT 6000

class EchoServer : CLanServer
{
public:
	EchoServer()
	{

	}

	EchoServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection)
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);

		int workCount = (int)si.dwNumberOfProcessors * 2;
		Start(ip, port, workCount, workCount - 2, true, 500);
	}

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		printf("EchoServer::Quit();\n");
		CLanServer::QuitServer();
	}

	virtual bool OnConnectionRequest(ULONG ip, LONG port)
	{
		return true;
	}

	virtual void OnAccept(ULONGLONG SessionID);

	virtual void OnRelease(ULONGLONG SessionID);

	virtual void OnRecv(ULONGLONG SessionID, RefCountPointer<CPacket> cpacket);

	virtual void OnError(int errorcode, WCHAR* message);
private:
};