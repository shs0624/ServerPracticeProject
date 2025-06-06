#pragma once
#include "CLanServer.h"

class EchoServer : CLanServer
{
public:
	EchoServer(UCHAR* ip, LONG port, bool bNagleEnabled, int maxConnection)
	{

	}

	virtual bool OnConnectionRequest(UCHAR* ip, LONG port)
	{

	}

	virtual void OnAccept()
	{

	}

	virtual void OnRelease(ULONG SessionID)
	{

	}

	virtual void OnRecv(ULONG SessionID, CPacket*)
	{

	}

	virtual void OnError(int errorcode, WCHAR* message)
	{

	}
private:
};