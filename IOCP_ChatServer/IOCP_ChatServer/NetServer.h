#pragma once
#include "CSerializationBuffer.h"
#include "RefCountPointer.h"
#include "CRingBuffer.h"
#include "LockFreeQueue.h"
#include "LockFreeStack_Re.h"
#define PROTOCOL_MAX_SIZE 156
#define SERVERPORT	6000
#define PROTOCOL_SIZE 10
#define PROTOCOL_NUMSIZE 8
#define FIXED_KEY 0xa9

#pragma pack(1)
struct st_NetHeader
{
	unsigned char FixedKey;
	short shLen;
	unsigned char RandKey;
	unsigned char CheckSum;
};
#pragma pack(pop)

struct st_Session
{
	OVERLAPPED sendOverlapped;
	OVERLAPPED recvOverlapped;
	ULONGLONG ulSessionID;
	SOCKET sock;
	LockFreeQueue<RefCountPointer>* sendBuf;
	CRingBuffer* recvBuf;
	RefCountPointer cPacketArr[200];

	DWORD dwSendCount;
	alignas(4) DWORD dwIOCount;
	BOOL bReleaseFlag;
	BOOL bSendFlag;
	BOOL bCanceled;
};

class CNetServer
{
public:
	bool StartNetServer(ULONG ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, int maxConnection);
	virtual void QuitServer();

	bool DecrementIOCount(st_Session* ptr);
	bool Disconnect(ULONGLONG sessionID);

	bool SendPacket_UniCast(ULONGLONG sessionID, RefCountPointer& cPacket);
	bool SendPacket_MultiCast(ULONGLONG sessionID, RefCountPointer& cPacket);

	int getAcceptTPS() { return _iAcceptTPS; }
	int getRecvMessageTPS() { return _iRecvMessageTPS; }
	int getSendMessageTPS() { return _iSendMessageTPS; }

	virtual bool OnConnectionRequest(ULONG ip, LONG port) = 0;

	virtual bool OnAccept(ULONGLONG sessionID) = 0;

	virtual void OnRelease(ULONGLONG SessionID) = 0;

	virtual void OnRecv(ULONGLONG SessionID, RefCountPointer& cpacket) = 0;

	virtual void OnError(int errorcode, WCHAR* message) = 0;
protected:
	int _workerCount;
	int _iSessionCount;
	int _imaxConnection;
	int _iAcceptTPS;
	int _iRecvMessageTPS;
	int _iSendMessageTPS;

	HANDLE _hTPSUpdateEvent;

	ULONG _threadID = 1;
	st_Session* _sessionArr;

	LockFreeStack<ULONGLONG> _emptyIndexStack;

	// 초기화 함수
	void InitializeSessions(ULONG maxConnection);
	bool Init(int maxConnection);

	int FindUsableSessionIndex();
	void FindSession(ULONGLONG sessionID, st_Session** ptr);

	// 스레드 함수들
	static unsigned int WINAPI TPSThread(LPVOID arg);
	static unsigned int WINAPI AcceptThread(LPVOID arg);
	static unsigned int WINAPI IOCPWorkerThread(LPVOID arg);

	// 메세지 처리를 위한 함수
	bool AcceptProc(CNetServer* thisPtr);
	bool SetWSARecv(st_Session* ptr);
	bool SetWSASend(st_Session* ptr);
	bool RecvProc_Net(st_Session* ptr, DWORD cbTransferred);
	void ReleaseSession(ULONGLONG ulSessionID);
	void ResetTPS();
};