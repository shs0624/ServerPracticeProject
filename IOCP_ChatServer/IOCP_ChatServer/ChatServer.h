#pragma once
#define dfSECTOR_MAX_Y 50
#define dfSECTOR_MAX_X 50
#define dfSLEEPTIME 500
#define dfTIMEOUT_SESSION 5000
#define dfTIMEOUT_USER 40000

// 로그인 하지 않은 세션
struct st_SESSION
{
	ULONGLONG ulSessionID;

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
	bool bDeleted;
};

// 로그인 한 유저
struct st_USER
{
	ULONGLONG ulSessionID;
	INT64 AccountNum;

	WCHAR ID[20];
	WCHAR NickName[20];
	char SessionKey[64];

	WORD sectorX;
	WORD sectorY;

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
	bool bDeleted;
};

class ChatServer : CNetServer
{
public:
	ChatServer()
	{
		
	}

	ChatServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection)
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);

		_emptyIndexStack = new LockFreeStack<ULONGLONG>();

		int workCount = (int)si.dwNumberOfProcessors * 2;
		StartNetServer(ip, port, workCount, workCount - 2, true, 5000);

		_UserPool = new procademy::CMemoryPool<st_USER>(10000, false, false);
		_SessionPool = new procademy::CMemoryPool<st_SESSION>(12000, false, false);
		_MessageQ = new LockFreeQueue<RefCountPointer>();

		_hMessageQueueEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		_hTimeoutEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		_ContentsThreadHandle = (HANDLE)_beginthreadex(NULL, 0, ContentsThread, this, 0, &_ContentsThreadID);
	}

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CNetServer::QuitServer();
	}

	//virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG SessionID);
	virtual void OnRelease(ULONGLONG SessionID);
	virtual void OnRecv(ULONGLONG SessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message);
private:
	void InitChatServer();
	// time 측정을 위한 Update
	void TimeCheck(DWORD& sleepTime);

	// 프레임 스킵 함수
	bool Skip();

	void PacketProc(RefCountPointer& cPacket, unordered_set<ULONGLONG>* pendingIDSet);

	void WorkProc(RefCountPointer& cPacket, WORD workType);

	void MessageProc();

	void MessageProc_Login(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID);

	void MessageProc_Move(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID, unordered_set<ULONGLONG>* pendingIDSet);

	void MessageProc_Message(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID, unordered_set<ULONGLONG>* pendingIDSet);

	void mpRESLogin(RefCountPointer& cPacket, BYTE status, INT64 accountNum);

	void mpRESSectorMove(RefCountPointer& cPacket, INT64 accountNum, WORD sectorX, WORD sectorY);

	void mpRESMessage(RefCountPointer& cPacket, INT64 accountNum, WCHAR* id, WCHAR* nick, WORD len, WCHAR* message);

	void DisconnectDeletedSession();

	//void MoveSector(ULONGLONG ulSessionID);

	//void SendAroundSector(ULONGLONG ulSessionID, RefCountPointer& cpacket);

	static unsigned int WINAPI ContentsThread(LPVOID arg);

	procademy::CMemoryPool<st_USER>* _UserPool;
	procademy::CMemoryPool<st_SESSION>* _SessionPool;
	
	HANDLE _ContentsThreadHandle;
	unsigned int _ContentsThreadID;

	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;
	HANDLE _hMessageQueueEvent;
	LockFreeQueue<RefCountPointer>* _MessageQ;

	// AccountNum, 유저 구조체
	unordered_map<ULONGLONG, st_USER*> _UserMap;
	// SessionID, 세션 구조체
	unordered_map<ULONGLONG, st_SESSION*> _SessionMap;

	// 섹터 관리
	vector<st_USER*> _SectorVector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
};