#pragma once
#define dfSECTOR_MAX_Y 32
#define dfSECTOR_MAX_X 32
#define dfTIMEOUT_SESSION 3000
#define dfTIMEOUT_USER 40000

struct st_CHARACTER
{
	ULONGLONG ulSessionID;
	INT64 AccountNum;

	WCHAR ID[20];
	WCHAR NickName[20];
	char SessionKey[64];

	short sectorX;
	short sectorY;

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
	bool bDeleted;
};

class ChatServer : CNetServer
{
public:
	ChatServer()
	{
		_ContentsThreadHandle = (HANDLE)_beginthreadex(NULL, 0, ContentsThread, this, 0, &_ContentsThreadID);
	}

	ChatServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection)
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);

		int workCount = (int)si.dwNumberOfProcessors * 2;
		StartNetServer(ip, port, workCount, workCount - 2, true, 500);
	}

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CNetServer::QuitServer();
	}

	virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG SessionID);
	virtual void OnRelease(ULONGLONG SessionID);
	virtual void OnRecv(ULONGLONG SessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message);
private:
	// time 측정을 위한 Update
	void TimeCheck();

	// 프레임 스킵 함수
	bool Skip();

	void MessageProc();

	void DisconnectDeletedSession();

	void MoveSector(ULONGLONG ulSessionID);

	void SendAroundSector(ULONGLONG ulSessionID, RefCountPointer& cpacket);

	static unsigned int WINAPI ContentsThread(LPVOID arg);
	
	HANDLE _ContentsThreadHandle;
	unsigned int _ContentsThreadID;

	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;
	HANDLE _hMessageQueueEvent;
	LockFreeQueue<RefCountPointer>* _MessageQ;

	std::queue<st_CHARACTER*> _TimeoutQ;

	// 섹터 관리
	vector<st_CHARACTER*> m_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
};