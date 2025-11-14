#pragma once
#include "PacketDefine.h"
#include "CSerializationBuffer.h"
#define PROTOCOL_MAX_SIZE 156
#define SERVERPORT	6000
#define PROTOCOL_SIZE 10
#define PROTOCOL_NUMSIZE 8
#define FIXED_KEY 0xa9

//#pragma pack(1)
struct st_Session
{
	OVERLAPPED sendOverlapped;
	OVERLAPPED recvOverlapped;
	SOCKET sock;
	std::queue<RefCountPointer>* sendBuf;
	CRingBuffer* recvBuf;
	std::queue<RefCountPointer>* cPacketQ;
	//RefCountPointer cPacketArr[200];

	DWORD dwSendCount;
	BOOL bSendFlag;
	BOOL bConnected;
};
//#pragma pack(pop)

class CNetClient
{
public:
	bool StartNetClient();

	bool Connect(SOCKADDR_IN _serverAddr);
	bool Disconnect();

	bool SendPacket_Re(DWORD& sendTPS);
	bool SendPacket_UniCast(RefCountPointer& cPacket);
	bool SendPost(LPVOID ptr, RefCountPointer& cPacket, HANDLE iocpHandle);

	virtual bool OnRecv(RefCountPointer& cPacket) = 0;

	virtual bool OnSend() = 0;

	/*virtual bool OnConnectionRequest(ULONG ip, LONG port) = 0;

	virtual bool OnAccept(ULONGLONG sessionID) = 0;

	virtual void OnRelease(ULONGLONG SessionID) = 0;

	virtual void OnError(int errorcode, WCHAR* message) = 0;*/
protected:
	st_Session* _mySession;

	ULONG _threadID = 1;

	SOCKADDR_IN _serverAddr;

	// 초기화 함수
	bool Init(SOCKADDR_IN serverAddr);

	// 메세지 처리를 위한 함수
	bool SetWSARecv();
	bool SetWSASend();
	bool RecvProc_Net(DWORD cbTransferred, DWORD& recvTPS);
};