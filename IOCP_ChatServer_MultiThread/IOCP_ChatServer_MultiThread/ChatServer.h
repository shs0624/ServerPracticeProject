#pragma once
#define dfSECTOR_MAX_Y 50
#define dfSECTOR_MAX_X 50
#define dfSLEEPTIME 1000
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
	bool bBatched;
};

class ChatServer : CNetServer
{
public:
	ChatServer()
	{
		
	}

	ChatServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection)
	{
		InitChatServer();

		SYSTEM_INFO si;
		GetSystemInfo(&si);

		_emptyIndexStack = new LockFreeStack<ULONGLONG>();

		int workCount = (int)si.dwNumberOfProcessors * 2;
		StartNetServer(ip, port, workCount, workCount - 2, true, 5000);

		_UserPool = new procademy::CMemoryPool<st_USER>(10000, false, false);
		_SessionPool = new procademy::CMemoryPool<st_SESSION>(12000, false, false);
	}

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CNetServer::QuitServer();
	}

	//virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG sessionID);
	virtual void OnRelease(ULONGLONG sessionID);
	virtual void OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message);
private:
	void InitChatServer();
	// time 측정을 위한 Update
	void TimeCheck(DWORD& sleepTime);

	// 프레임 스킵 함수
	bool Skip();

	void PacketProc(RefCountPointer& cPacket);

	void MessageProc();

	void MessageProc_Login(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID);

	void MessageProc_Move(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID);

	void MessageProc_Message(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID);

	void mpRESLogin(RefCountPointer& cPacket, BYTE status, INT64 accountNum);

	void mpRESSectorMove(RefCountPointer& cPacket, INT64 accountNum, WORD sectorX, WORD sectorY);

	void mpRESMessage(RefCountPointer& cPacket, INT64 accountNum, WCHAR* id, WCHAR* nick, WORD len, WCHAR* message);

	void SendPacket_Sector(RefCountPointer& cPacket, WORD sectorX, WORD sectorY);

	void LockSectorMove(WORD sectorX, WORD sectorY, WORD nSectorX, WORD nSectorY);
	
	void UnLockSectorMove(WORD sectorX, WORD sectorY, WORD nSectorX, WORD nSectorY);

	void DisconnectDeletedSession();

	static unsigned int WINAPI TimerThread(LPVOID arg);

	//void MoveSector(ULONGLONG ulSessionID);

	//void SendAroundSector(ULONGLONG ulSessionID, RefCountPointer& cpacket);

	procademy::CMemoryPool<st_USER>* _UserPool;
	procademy::CMemoryPool<st_SESSION>* _SessionPool;
	
	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;
	HANDLE _hMessageQueueEvent;

	HANDLE _TimerThreadHandle;
	unsigned int _TimerThreadID;

	// AccountNum, 유저 구조체
	unordered_map<ULONGLONG, st_USER*> _AccountNumUserMap;
	SRWLOCK _AccountNumUserMapLock;

	// SessionID, 유저 구조체
	unordered_map<ULONGLONG, st_USER*> _UserMap;
	SRWLOCK _UserMapLock;

	// SessionID, 세션 구조체
	unordered_map<ULONGLONG, st_SESSION*> _SessionMap;
	SRWLOCK _SessionMapLock;

	// 섹터 관리 - sessionID
	vector<ULONGLONG> _SectorVector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
	SRWLOCK _SectorLock[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
};