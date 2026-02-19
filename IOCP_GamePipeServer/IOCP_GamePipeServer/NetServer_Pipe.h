#pragma once
#define PROTOCOL_MAX_SIZE 500
#define MAX_PACKET_BATCH 1000
#define SERVERPORT	20208
#define PROTOCOL_SIZE 10
#define PROTOCOL_NUMSIZE 8
#include "LogManager.h"

#include <cpp_redis/cpp_redis>
#include <tacopie/tacopie>
#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")

#pragma pack(push, 1)
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
	ULONGLONG ulSessionID;
	CPacketRingBuffer* _MessageQ;
	DWORD dwSendCount;
	alignas(4) DWORD dwIOCount;
	BOOL bReleaseFlag;
	BOOL bSendFlag;
	BOOL bCanceled;

	LockFreeQueue<RefCountPointer>* sendBuf;
	CRingBuffer* recvBuf;
	OVERLAPPED sendOverlapped;
	OVERLAPPED recvOverlapped;
	RefCountPointer* cPacketArr;

	// 소속 Room 정보를 번호로 할지 뭐로할지..
	LPVOID pUser;
	DWORD dwIncludedRoom;

	SOCKET sock;
	SOCKADDR_IN clientAddr;
};

class CNetServer
{
public:
	CNetServer() {};

	bool StartNetServer(ULONG ip, LONG port, int concurrentThreads, bool bNagleEnabled, int maxConnection, unsigned char programKey, unsigned char fixedKey);
	virtual void QuitServer();

	bool DecrementIOCount(st_NetSession* ptr);
	bool Disconnect(ULONGLONG sessionID);
	bool GetClientAddr(ULONGLONG sessionID, WCHAR* buffer, int len);
	bool SetInfoToSession(ULONGLONG sessionID, LPVOID ptr, DWORD roomNum);
	bool GetInfoFromSession(ULONGLONG sessionID, LPVOID* ptr);

	bool PostSend(ULONGLONG sessionID);

	bool EnqueueSendBuffer(ULONGLONG sessionID, RefCountPointer& cPacket, bool pushHeader = true);

	bool PostPacket(ULONGLONG sessionID, RefCountPointer& cPacket, bool pushHeader = true)
	{
		st_NetSession* ptr;
		FindSession(sessionID, &ptr);
		if (ptr == NULL)
		{
			return false;
		}

		InterlockedIncrement(&ptr->dwIOCount);
		if (ptr->bReleaseFlag == 1)
		{
			DecrementIOCount(ptr);
			return false;
		}

		if (sessionID != ptr->ulSessionID)
		{
			DecrementIOCount(ptr);
			return false;
		}

		if (pushHeader)
		{
			short shSize = (*cPacket)->GetDataSize();

			st_NetHeader netHeader;
			netHeader.FixedKey = _ProgramKey;
			netHeader.RandKey = (unsigned char)rand() % 256;
			netHeader.shLen = shSize;

			(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
			(*cPacket)->Encode(_FixedKey, netHeader.RandKey);
		}

		ptr->sendBuf->Enqueue(cPacket);

		InterlockedIncrement(&ptr->dwIOCount);
		PostQueuedCompletionStatus(_NetIOCPHandle, 1, (ULONG_PTR)ptr, &_SendOverlapped);

		_pLog._dwSendMessageTPS++;
		DecrementIOCount(ptr);
		return true;
	}

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
	friend class IRoom;

	int _workerCount;
	int _iSessionCount;
	int _imaxConnection;
	int _iAcceptTPS;
	int _iRecvMessageTPS;
	int _iSendMessageTPS;

	unsigned char _FixedKey;
	unsigned char _ProgramKey;

	OVERLAPPED _ReleaseOverlapped;
	OVERLAPPED _SendOverlapped;

	// 비정적 멤버는 인스턴스마다 다른 메모리를 가지는데, thread_local은
	// 인스턴스마다가 아니라, 스레드 마다 같은 메모리를 가지니 의미가 충돌한다.
	// 그래서 static으로 선언해야 한다.
	static thread_local stChatLog _pLog;

	SOCKET _ListenSocket;

	HANDLE _acceptThreadHandle;
	HANDLE _NetIOCPHandle;
	HANDLE _NetIOCPWorkerThreadHandleArr[50];

	unsigned int _acceptThreadID;
	unsigned int _NetIOCPWorkerThreadID[50];

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
	bool CheckSession(st_NetSession* ptr, ULONGLONG sessionID);

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