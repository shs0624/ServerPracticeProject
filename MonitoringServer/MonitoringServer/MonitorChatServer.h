#define dfTIMEOUT_SESSION 40000
#define dfSERVERPORT_CHAT 20221

struct st_ChatSESSION
{
	ULONGLONG ulSessionID;
	SOCKADDR_IN ClientAddr;

	DWORD dwLastRecvTime;
};

// 로그인 한 유저 - 채팅서버는 serverNum
struct st_ChatUSER
{
	ULONGLONG ulSessionID;
	int iServerNum;
	SOCKADDR_IN ClientAddr;

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
};

// 모니터링 클라이언트에게 데이터를 전달하는 서버
class MonitorChatServer : CLanServer
{
public:
	MonitorChatServer()
	{

	}

	void InitMonitorChatServer(MonitorDataManager* pManager, ULONG ip, int maxConnection, unsigned char programKey, unsigned char fixedKey);

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CLanServer::QuitServer();
	}

	//virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr);
	virtual void OnRelease(ULONGLONG sessionID);
	virtual void OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message);
private:
	// time 측정을 위한 Update
	void TimeCheck(DWORD& sleepTime);

	void mpRESLogin(RefCountPointer& cPacket, BYTE status, INT64 accountNum);

	void mpRESSectorMove(RefCountPointer& cPacket, INT64 accountNum, WORD sectorX, WORD sectorY);

	void mpRESMessage(RefCountPointer& cPacket, INT64 accountNum, WCHAR* id, WCHAR* nick, WORD len, WCHAR* message);

	void MessageProc_ServerMonitorLogin(RefCountPointer& cPacket, ULONGLONG sessionID);

	void MessageProc_UpdateMonitorData(RefCountPointer& cPacket, ULONGLONG sessionID);

	static unsigned int WINAPI TimerThread(LPVOID arg);

	procademy::CMemoryPool_LockFree<st_ChatUSER>* _UserPool;
	procademy::CMemoryPool_LockFree<st_ChatSESSION>* _SessionPool;

	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;
	HANDLE _hMessageQueueEvent;

	HANDLE _TimerThreadHandle;
	unsigned int _TimerThreadID;

	MonitorDataManager* _pMonitorDataManager;

	// SessionID, 유저 구조체
	unordered_map<ULONGLONG, st_ChatUSER*> _UserMap;
	SRWLOCK _UserMapLock;

	// SessionID, 세션 구조체
	unordered_map<ULONGLONG, st_ChatSESSION*> _SessionMap;
	SRWLOCK _SessionMapLock;
};