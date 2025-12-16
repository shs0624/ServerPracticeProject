#pragma once
#define PROTOCOL_MAX_SIZE 500
#define SERVERPORT	10004
#define PROTOCOL_SIZE 10
#define PROTOCOL_NUMSIZE 8
#define FIXED_KEY 0x32
#define PROGRAM_KEY 0x77
#include "LogManager.h"

#include <cpp_redis/cpp_redis>
#include <tacopie/tacopie>
#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")

#pragma pack(1)
struct st_NetHeader
{
	unsigned char FixedKey;
	short shLen;
	unsigned char RandKey;
	unsigned char CheckSum;
};
#pragma pack(pop)

struct st_NetSession
{
	OVERLAPPED sendOverlapped;
	OVERLAPPED recvOverlapped;
	ULONGLONG ulSessionID;
	SOCKET sock;
	SOCKADDR_IN clientAddr;
	LockFreeQueue<RefCountPointer>* sendBuf;
	CRingBuffer* recvBuf;
	RefCountPointer cPacketArr[200];

	DWORD dwSendCount;
	alignas(4) DWORD dwIOCount;
	BOOL bReleaseFlag;
	BOOL bSendFlag;
	BOOL bCanceled;
	BOOL bDeleted;
};

class CNetServer
{
public:
	CNetServer() {};

	bool StartNetServer(ULONG ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, int maxConnection);
	virtual void QuitServer();

	bool DecrementIOCount(st_NetSession* ptr);
	bool Disconnect(ULONGLONG sessionID);
	bool GetClientAddr(ULONGLONG sessionID, WCHAR* buffer, int len);

	bool SendPost(ULONGLONG sessionID);
	bool EnqueueSendBuffer(ULONGLONG sessionID, RefCountPointer& cPacket, bool pushHeader = true);
	bool PostPacket(ULONGLONG sessionID, RefCountPointer& cPacket, bool pushHeader = true);
	bool SendPacket_UniCast(ULONGLONG sessionID, RefCountPointer& cPacket, bool pushHeader = true);
	bool SendPacket_MultiCast(ULONGLONG* sessionIDArr, WORD count, RefCountPointer& cPacket);

	int getAcceptTPS() { return _iAcceptTPS; }
	int getRecvMessageTPS() { return _iRecvMessageTPS; }
	int getSendMessageTPS() { return _iSendMessageTPS; }

	//virtual bool OnConnectionRequest(ULONG ip, LONG port) = 0;
	virtual bool OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr) = 0;

	virtual void OnRelease(ULONGLONG sessionID) = 0;

	virtual void OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket) = 0;

	virtual void OnError(int errorcode, WCHAR* message) = 0;
protected:
	int _workerCount;
	int _iSessionCount;
	int _imaxConnection;
	int _iAcceptTPS;
	int _iRecvMessageTPS;
	int _iSendMessageTPS;

	OVERLAPPED _ReleaseOverlapped;

	// 비정적 멤버는 인스턴스마다 다른 메모리를 가지는데, thread_local은
	// 인스턴스마다가 아니라, 스레드 마다 같은 메모리를 가지니 의미가 충돌한다.
	// 그래서 static으로 선언해야 한다.
	static thread_local stChatLog _pLog;

	HANDLE _hTPSUpdateEvent;

	ULONG _threadID = 1;
	st_NetSession* _sessionArr;

	LockFreeStack<ULONGLONG>* _emptyIndexStack;

	//cpp_redis::client* _pRedisClient;

	// 초기화 함수
	void InitializeSessions(ULONG maxConnection);
	bool Init(int maxConnection);

	void PostRelease(st_NetSession* ptr);

	int FindUsableSessionIndex();
	void FindSession(ULONGLONG sessionID, st_NetSession** ptr);

	// 스레드 함수들
	static unsigned int WINAPI AcceptThread(LPVOID arg);
	static unsigned int WINAPI IOCPWorkerThread(LPVOID arg);

	// 메세지 처리를 위한 함수
	bool AcceptProc(CNetServer* thisPtr);
	bool SetWSARecv(st_NetSession* ptr);
	bool SetWSASend(st_NetSession* ptr);
	bool RecvProc_Net(st_NetSession* ptr, DWORD cbTransferred);
	void ReleaseSession(ULONGLONG ulSessionID);
};