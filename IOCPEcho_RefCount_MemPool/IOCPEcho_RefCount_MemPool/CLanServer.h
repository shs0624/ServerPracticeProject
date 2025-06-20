#pragma once
#include "CPacket_Mempool.h"
#include "CRingBuffer.h"
#include "RefCountPointer.h"
//#include <stack>
#include "TestStack.h"
#include <deque>

#define PROTOCOL_MAX_SIZE 16
#define SEND_MAX 150
//#define CHECKPROFILE

#pragma pack(1)
struct st_NetHeader
{
	short shLen;
};
#pragma pack(pop)

struct st_Session
{
	OVERLAPPED sendOverlapped;
	OVERLAPPED recvOverlapped;

	// 상위 2바이트 = 인덱스, 하위 6바이트 = 세션ID
	ULONGLONG ulSessionID;
	SOCKET sock;
	//CRingBuffer* sendBuf;
	CRingBuffer* recvBuf;
	std::deque<RefCountPointer<CPacket>> sendBuf;

	DWORD dwIOCount;
	DWORD dwSendCount;
	BOOL bSendFlag;
	BOOL bSessionUsing;
	CRITICAL_SECTION crtLock;
};

class CLanServer
{
public:
	bool Start(ULONG ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, WORD maxConnection);
	void Stop();
	int GetSessionCount();
	virtual void QuitServer();

	bool Disconnect(ULONGLONG sessionID);
	bool SendPacket(ULONGLONG sessionID, RefCountPointer<CPacket> cPacket);

	int getAcceptTPS() { return _iAcceptTPS; }
	int getRecvMessageTPS() { return _iRecvMessageTPS; }
	int getSendMessageTPS() { return _iSendMessageTPS; }

	virtual bool OnConnectionRequest(ULONG ip, LONG port) = 0; // accept 직후 핸들러
	//virtual void OnClientJoin(Client 정보 / SessionID / 기타등등) = 0;
	virtual void OnAccept() = 0; // Accept 후 접속처리 완료 후 호출

	virtual void OnRelease(ULONGLONG SessionID) = 0;

	virtual void OnRecv (ULONGLONG SessionID, RefCountPointer<CPacket> cpacket) = 0;
	//virtual void OnMessage() = 0;

	//	virtual void OnWorkerThreadBegin() = 0;                    < 워커스레드 GQCS 바로 하단에서 호출
	//	virtual void OnWorkerThreadEnd() = 0;                      < 워커스레드 1루프 종료 후
	virtual void OnError(int errorcode, WCHAR* message) = 0; 

	/*static void* operator new(size_t size)
	{

	}*/
protected:
	int _workerCount;
	int _iSessionCount;
	int _imaxConnection;
	int _iAcceptTPS;
	int _iRecvMessageTPS;
	int _iSendMessageTPS;
	int _iReleaseTPS;

	HANDLE _hTPSUpdateEvent;

	// 상위 2바이트 = 인덱스 / 하위 6바이트는 스레드ID
	ULONGLONG _threadID = 0;
	st_Session* _sessionArr;
	//std::stack<WORD> _indexStack;
	TestStack<ULONGLONG> _indexStack;

	// 초기화 함수
	void InitializeSessions(WORD maxConnection);

	// 스레드 함수들
	static unsigned int WINAPI TPSThread(LPVOID arg);
	static unsigned int WINAPI AcceptThread(LPVOID arg);
	static unsigned int WINAPI IOCPWorkerThread(LPVOID arg);

	// 메세지 처리를 위한 함수
	void GetSession(ULONGLONG ulSessionID, st_Session** pSession);
	bool AcceptProc(CLanServer* thisPtr);
	bool SetWSARecv(st_Session* ptr);
	bool SetWSASend(st_Session* ptr);
	bool RecvProc(st_Session* ptr, DWORD cbTransferred);
	void ReleaseSession(st_Session* ptr);
	void ResetTPS();
};