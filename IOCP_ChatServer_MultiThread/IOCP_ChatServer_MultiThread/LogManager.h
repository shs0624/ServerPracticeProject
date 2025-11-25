#pragma once
#define dfLOG_MAX 10000

class LogController
{
public:
	void Init();

	void PrintLog();

	DWORD _dwRecvMessageTPS;
	DWORD _dwSendMessageTPS;

	DWORD _dwAcceptTotal;
	DWORD _dwAcceptTPS;
	DWORD _dwUpdateTPS;
	DWORD _dwUpdateQSize;

	DWORD _dwSessionCount;
	DWORD _dwUserCount;

	DWORD _dwPacketPoolUse;
	DWORD _dwPlayerPoolUse;

	DWORD _dwMoveMessageTPS;
	DWORD _dwChatMessageTPS;
	DWORD _dwLoginMessageTPS;

	DWORD _dwTimeoutSessionTotal;
	DWORD _dwTimeoutUserTotal;

	static LogController _LogController;
protected:
	static unsigned int WINAPI LogingThread(LPVOID arg);

	HANDLE _hLogUpdateEvent;
	HANDLE _htpsThreadHandle;
	unsigned int _tpsThreadID;


	DWORD _iLogCount;
};