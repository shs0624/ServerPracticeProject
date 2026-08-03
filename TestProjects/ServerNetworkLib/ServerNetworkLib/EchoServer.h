#pragma once
#include "CLanServer.h"

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

	virtual bool OnConnectionRequest(ULONG ip, LONG port)
	{
		return true;
	}

	virtual void OnAccept();

	virtual void OnRelease(ULONG SessionID);

	virtual void OnRecv(ULONG SessionID, CPacket* cpacket);

	virtual void OnError(int errorcode, WCHAR* message);
private:
};