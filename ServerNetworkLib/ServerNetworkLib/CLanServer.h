#pragma once
#include "CSerializationBuffer.h"
#include "CRingBuffer.h"
#define PROTOCOL_MAX_SIZE 16

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
	DWORD dwSessionID;
	SOCKET sock;
	CRingBuffer* sendBuf;
	CRingBuffer* recvBuf;

	CRITICAL_SECTION CrtLock;
	DWORD dwIOCount;
	BOOL bSendFlag;
};

class CLanServer
{
public:
	bool Start(UCHAR* ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, int maxConnection);
	void Stop();
	int GetSessionCount();

	bool Disconnect(ULONG sessionID);
	bool SendPacket(ULONG sessionID, CPacket* cPacket);

	virtual bool OnConnectionRequest(UCHAR* ip, LONG port) = 0; // accept 직후 핸들러
	//virtual void OnClientJoin(Client 정보 / SessionID / 기타등등) = 0;
	virtual void OnAccept() = 0;

	//virtual void OnClientLeave(ULONG SessionID) = 0;
	virtual void OnRelease(ULONG SessionID) = 0;

	virtual void OnRecv(ULONG SessionID, CPacket*) = 0;
	//virtual void OnMessage() = 0;

	//	virtual void OnSend(SessionID, int sendsize) = 0;           < 패킷 송신 완료 후

	//	virtual void OnWorkerThreadBegin() = 0;                    < 워커스레드 GQCS 바로 하단에서 호출
	//	virtual void OnWorkerThreadEnd() = 0;                      < 워커스레드 1루프 종료 후
	virtual void OnError(int errorcode, WCHAR* message) = 0; 
private:
	// 비동기 입출력 처리 함수
	static unsigned int WINAPI AcceptThread(LPVOID arg);
	static unsigned int WINAPI IOCPWorkerThread(LPVOID arg);

	/*SOCKET listen_sock;

	HANDLE _acceptThreadHandle;
	HANDLE _iocpHandle;
	HANDLE _iocpWorkerThreadHandleArr[50];

	unsigned int _acceptThreadID;
	unsigned int _iocpWorkerThreadID[50];

	DWORD _threadID = 0;
	CRITICAL_SECTION _sessionMapLock;
	std::unordered_map<DWORD, st_Session*> _sessionMap;*/
};