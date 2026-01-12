#define dfTIMEOUT_SESSION 40000
#define dfSERVERPORT_CLIENT 13004

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
class MonitorClientServer : CLanServer
{
public:
	MonitorClientServer()
	{

	}

	void InitMonitorClientServer(ULONG ip, int maxConnection, unsigned char programKey, unsigned char fixedKey);

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CLanServer::QuitServer();
	}

	void UpdateAll()
	{

	}

	void Update(BYTE serverNum, BYTE dataType, int dataValue, int timeStamp)
	{
		RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
		(*cPacket)->Initialize(sizeof(st_LanHeader));
		_pLog._dwPacketPoolUse++;

		mpDataUpdate(cPacket, serverNum, dataType, dataValue, timeStamp);

		// 직접 패킷 인코딩까지
		st_LanHeader lanHeader;
		lanHeader.FixedKey = _ProgramKey;
		lanHeader.RandKey = (unsigned char)rand() % 256;
		lanHeader.shLen = (*cPacket)->GetDataSize();

		(*cPacket)->PushHeader((char*)&lanHeader, sizeof(st_LanHeader));
		(*cPacket)->Encode(_FixedKey, lanHeader.RandKey);

		// 연결된 클라에게 전송
		for (auto it = _UserMap.begin(); it != _UserMap.end(); it++)
		{
			cPacket.IncRefCount();
			SendPacket_UniCast((*it).second->ulSessionID, cPacket, false);
		}

		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		return;
	}

	//virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr);
	virtual void OnRelease(ULONGLONG sessionID);
	virtual void OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message);
private:
	// time 측정을 위한 Update
	void TimeCheck(DWORD& sleepTime);

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