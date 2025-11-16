#pragma once
#define dfLOG_MAX 10000

enum LOG_TYPE
{
	SEND,
	RECV
};

struct stLOG
{
	LOG_TYPE type;
	INT64 AccountNo;
	INT64 recvNo;
	en_PACKET_TYPE packetType;
};

class LogController
{
public:
	void Init(SOCKADDR_IN serverAddr);

	void PrintLog();

	void LOG_SEND(INT64 accountNo, WORD packetType);

	void LOG_RECV(INT64 accountNo, INT64 recvNo, WORD packetType);

	DWORD _dwRecvMessageTPS;
	DWORD _dwSendMessageTPS;

	DWORD _dwConnectTry;
	DWORD _dwConnectSuccess;
	DWORD _dwConnectFail;

	DWORD _dwLoginSendCount;
	DWORD _dwMoveSendCount;
	DWORD _dwChatSendCount;

	DWORD _dwConnectWaitCount;
	DWORD _dwLoginWaitCount;
	DWORD _dwDisconnectFromServerCount;
	DWORD _dwResponseFailCount;
	DWORD _dwMessageNotRecvCount;
	DWORD _dwLoginResNotRecvCount;
	DWORD _dwNeedTimeoutSessionCount;
	DWORD _dwNeedTimeoutUserCount;

	DWORD _dwNormalDisconnectCount;
	DWORD _dwIntendedDisconnectSessionCount;

	static LogController _LogController;
protected:
	static unsigned int WINAPI LogingThread(LPVOID arg);

	HANDLE _hLogUpdateEvent;
	HANDLE _htpsThreadHandle;
	unsigned int _tpsThreadID;

	SOCKADDR_IN _serverAddr;

	int _iSessionCount;
	int _iThreadCount;

	DWORD _iLogCount;
	stLOG _logArr[10000];
};