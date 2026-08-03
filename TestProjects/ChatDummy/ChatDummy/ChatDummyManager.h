#pragma once
#define TIME_LOGINWAIT_EVENT 5000
#define TIME_CHAT_EVENT 5000
#define TIME_MOVE_EVENT 1000
#define TIME_LOGIN_EVENT 500

#define SERVERIP "127.0.0.1"
#define SERVERPORT 6000

class ChatDummyManager
{
public:
	ChatDummyManager()
	{
		
	}

	bool InitManager(string serverIP, int serverPort, int threadCount, int sessionCount);

	bool DecrementIOCount(st_Session* ptr);
	bool Disconnect(ULONGLONG sessionID);

	bool WorkByAction(ChatDummy* ptr);

	//int getAcceptTPS() { return _iAcceptTPS; }
	int getRecvMessageTPS() { return _dwRecvMessageTPS; }
	int getSendMessageTPS() { return _dwSendMessageTPS; }

	/*virtual bool OnConnect(ULONGLONG sessionID) = 0;

	virtual void OnRelease(ULONGLONG SessionID) = 0;

	virtual void OnRecv(ULONGLONG SessionID, RefCountPointer& cpacket) = 0;

	virtual void OnError(int errorcode, WCHAR* message) = 0;*/
protected:
	DWORD _dwLoopCount;

	DWORD _dwConnectTry;
	DWORD _dwConnectFail;
	DWORD _dwConnectSuccess;

	int _iSessionCount;
	int _iThreadCount;

	DWORD _dwRecvMessageTPS;
	DWORD _dwSendMessageTPS;

	DWORD _dwConnectWaitCount;
	DWORD _dwLoginWaitCount;
	DWORD _dwDisconnectFromServerCount;
	DWORD _dwResponseFailCount;
	DWORD _dwMessageNotRecvCount;
	DWORD _dwLoginResNotRecvCount;
	DWORD _dwNeedTimeoutSessionCount;
	DWORD _dwNeedTimeoutUserCount;

	SOCKADDR_IN _serverAddr;

	HANDLE _hMoveDummyEvent;
	HANDLE _hMoveDummyThreadHandle;
	unsigned int _MoveDummyThreadID;
	LockFreeQueue<ChatDummy*>* _MoveQueue;

	HANDLE _hChatDummyEvent;
	HANDLE _hChatDummyThreadHandle;
	unsigned int _ChatDummyThreadID;
	LockFreeQueue<ChatDummy*>* _ChatQueue;

	HANDLE _hLoginDummyEvent;
	HANDLE _hLoginThreadHandle;
	unsigned int _LoginThreadID;

	HANDLE _hLogUpdateEvent;
	HANDLE _htpsThreadHandle;
	unsigned int _tpsThreadID;

	HANDLE _hTimerEvent;
	HANDLE _hTimerThreadHandle;
	unsigned int _TimerThreadID;

	HANDLE _hHeartBeatEvent;
	HANDLE _hHeartBeatThreadHandle;
	unsigned int _HeartBeatThreadID;

	ULONG _threadID = 1;
	ChatDummy _DummyArr[1000];
	vector<ChatDummy*> _vTimeOutTargetVector;

	HANDLE _IOCPHandle;
	HANDLE _IOCPWorkerThreadHandleArr[100];
	unsigned int _IOCPWorkerThreadID[100];

	LPOVERLAPPED _lpWorkOverlapped;

	void PrintLog();
	void ResetTPS();

	// 초기화 함수
	void InitializeSessions(ULONG maxConnection);
	// 스레드 함수들
	static unsigned int WINAPI LogingThread(LPVOID arg);
	static unsigned int WINAPI MoveThread(LPVOID arg);
	static unsigned int WINAPI ChatThread(LPVOID arg);
	static unsigned int WINAPI TimerThread(LPVOID arg);
	static unsigned int WINAPI HeartBeatThread(LPVOID arg);
	static unsigned int WINAPI IOCPWorkerThread(LPVOID arg);
};