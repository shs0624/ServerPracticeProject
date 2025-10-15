#pragma once
#include "CSerializationBuffer.h"
#include "RefCountPointer.h"
#include "CRingBuffer.h"
#include "LockFreeQueue.h"
#include "TestStack.h"
#include <deque>
#define PROTOCOL_MAX_SIZE 16
#define SERVERPORT	6000
#define PROTOCOL_SIZE 10
#define PROTOCOL_NUMSIZE 8

#pragma pack(1)
struct st_NetHeader
{
	short shLen;
};

struct st_PACKET
{
	short shLen;
	LONGLONG llNum;
};

struct st_PACKET_HEADER
{
	ULONGLONG ulSessionID;
};
#pragma pack(pop)

struct st_Session
{
	OVERLAPPED sendOverlapped;
	OVERLAPPED recvOverlapped;
	ULONGLONG ulSessionID;
	SOCKET sock;
	//CRingBuffer* sendBuf;
	//std::deque<RefCountPointer<CPacket>> sendBuf;
	LockFreeQueue<RefCountPointer<CPacket>> sendBuf;
	CRingBuffer* recvBuf;
	//std::deque<LPVOID> recvBuf;

	DWORD dwSendCount;
	DWORD dwRecvCount;
	DWORD dwIOCount;
	BOOL bSendFlag;
	BOOL bSessionAlive;
	CRITICAL_SECTION sendLock;
};

class CLanServer
{
public:
	bool Start(ULONG ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, int maxConnection);
	void Stop();
	int GetSessionCount();
	virtual void QuitServer();

	bool Disconnect(ULONGLONG sessionID);
	bool SendPacket(ULONGLONG sessionID, RefCountPointer<CPacket> cPacket);

	int getAcceptTPS() { return _iAcceptTPS; }
	int getRecvMessageTPS() { return _iRecvMessageTPS; }
	int getSendMessageTPS() { return _iSendMessageTPS; }

	virtual bool OnConnectionRequest(ULONG ip, LONG port) = 0; // accept 직후 핸들러

	virtual void OnAccept(ULONGLONG sessionID) = 0; // Accept 후 접속처리 완료 후 호출

	virtual void OnRelease(ULONGLONG SessionID) = 0;

	virtual void OnRecv(ULONGLONG SessionID, CPacket* cpacket) = 0;
	//virtual void OnMessage() = 0;

	//	virtual void OnWorkerThreadBegin() = 0;                    < 워커스레드 GQCS 바로 하단에서 호출
	//	virtual void OnWorkerThreadEnd() = 0;                      < 워커스레드 1루프 종료 후
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

	TestStack<ULONGLONG> _emptyIndexStack;

	// 초기화 함수
	void InitializeSessions(ULONG maxConnection);
	bool Init(int maxConnection);

	int FindUsableSessionIndex();
	void FindSession(ULONGLONG sessionID, st_Session** ptr);

	// 스레드 함수들
	static unsigned int WINAPI TPSThread(LPVOID arg);
	static unsigned int WINAPI AcceptThread(LPVOID arg);
	static unsigned int WINAPI IOCPWorkerThread(LPVOID arg);
	static unsigned int WINAPI EchoThread(LPVOID arg);

	// 메세지 처리를 위한 함수
	void GetSession(ULONGLONG ulSessionID, st_Session** pSession);
	bool AcceptProc(CLanServer* thisPtr);
	bool SetWSARecv(st_Session* ptr);
	bool SetWSASend(st_Session* ptr);
	bool SendLoginPacket(ULONGLONG ulSessionID, CPacket* cPacket);
	bool RecvProc(st_Session* ptr, DWORD cbTransferred);
	void ReleaseSession(ULONGLONG ulSessionID);
	void ResetTPS();
}; 