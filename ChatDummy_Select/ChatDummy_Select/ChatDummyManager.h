#pragma once
#define TIME_LOGINWAIT_EVENT 5000
#define TIME_CHAT_EVENT 5000
#define TIME_MOVE_EVENT 1000
#define TIME_LOGIN_EVENT 500
#define df_FRAMETIME 333

#define SERVERIP "127.0.0.1"
#define SERVERPORT 6000

class ChatDummyManager
{
public:
	ChatDummyManager()
	{
		
	}

	bool InitManager(string serverIP, int serverPort, int threadCount, int sessionCount, bool bTimeoutTest, bool bMessageFloodTest);

	void OnOffManager();

	bool Skip();

	/*virtual bool OnConnect(ULONGLONG sessionID) = 0;

	virtual void OnRelease(ULONGLONG SessionID) = 0;

	virtual void OnRecv(ULONGLONG SessionID, RefCountPointer& cpacket) = 0;

	virtual void OnError(int errorcode, WCHAR* message) = 0;*/
protected:
	DWORD _dwLoopCount;

	int _iSessionCountPerThread;
	int _iThreadCount;
	int _iStartIdx;

	bool _bTestTimeout;
	bool _bTestMessageFlood;
	bool _bStop;

	SOCKADDR_IN _serverAddr;

	//HANDLE _hLogUpdateEvent;
	HANDLE _hChatDummyThreadHandle[5];
	unsigned int _ChatDummyThreadID[5];

	HANDLE _hLogUpdateEvent;
	HANDLE _htpsThreadHandle;
	unsigned int _tpsThreadID;

	ULONG _threadID = 1;
	ChatDummy _DummyArr[1000];

	// 스레드 함수
	static unsigned int WINAPI ChatDummyControlThread(LPVOID arg);
};