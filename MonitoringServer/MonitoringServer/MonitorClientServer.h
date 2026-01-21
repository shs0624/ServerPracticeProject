#define dfSLEEPTIME 1000
#define dfTIMEOUT_ClientSESSION 10000
#define dfSERVERPORT_CLIENT 20220
#define dfCLIENT_SESSIONKEY "ajfw@!cv980dSZ[fje#@fdj123948djf"

struct st_ClientSESSION
{
	ULONGLONG ulSessionID;
	SOCKADDR_IN ClientAddr;

	DWORD dwLastRecvTime;
};

// 로그인 한 유저
struct st_ClientUSER
{
	ULONGLONG ulSessionID;
	int iServerNum;
	SOCKADDR_IN ClientAddr;

	char SessionKey[32];

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
};

// 모니터링 클라이언트에게 데이터를 전달하는 서버
class MonitorClientServer : CNetServer
{
public:
	MonitorClientServer()
	{

	}

	void InitMonitorClientServer(ULONG ip, int maxConnection, unsigned char programKey, unsigned char fixedKey);

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CNetServer::QuitServer();
	}

	void UpdateAll()
	{

	}

	// IOCP가 아닌 MonitorDataManager가 호출하기 때문에, 그 로그의 TLS 로그주소 넘겨받기
	void Update(BYTE serverNum, BYTE dataType, int dataValue, int timeStamp, stChatLog* pLog);

	//virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr);
	virtual void OnRelease(ULONGLONG sessionID);
	virtual void OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message);
private:
	// time 측정을 위한 Update
	void TimeCheck();

	void mpLoginRES(RefCountPointer& cPacket, BYTE status);

	void mpDataUpdate(RefCountPointer& cPacket, BYTE serverNum, BYTE dataType, int dataValue, int timeStamp);

	void MessageProc_MonitorClientLogin(RefCountPointer& cPacket, ULONGLONG sessionID);

	static unsigned int WINAPI TimerThread(LPVOID arg);

	procademy::CMemoryPool_LockFree<st_ClientUSER>* _UserPool;
	procademy::CMemoryPool_LockFree<st_ClientSESSION>* _SessionPool;

	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;
	HANDLE _hMessageQueueEvent;

	HANDLE _TimerThreadHandle;
	unsigned int _TimerThreadID;

	// SessionID, 유저 구조체
	unordered_map<ULONGLONG, st_ClientUSER*> _UserMap;
	SRWLOCK _UserMapLock;

	// SessionID, 세션 구조체
	unordered_map<ULONGLONG, st_ClientSESSION*> _SessionMap;
	SRWLOCK _SessionMapLock;
};