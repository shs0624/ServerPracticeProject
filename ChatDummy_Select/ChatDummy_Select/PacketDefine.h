#pragma once
#define MAX_PROTOCOLSIZE 300
#define MAX_MESSAGELEN 64
#define FIXED_KEY 0xa9		// 인코딩용 고정키
#define PROGRAM_HEADER 0xbb	// 헤더에 포함할 프로그램 구분용 코드

#define dfSECTOR_MAX_Y 50
#define dfSECTOR_MAX_X 50

#define dfSessionTimeOutClient 25
#define dfUserTimeOutClient 25

#pragma pack(push,1)
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
	DWORD sessionID;
	SOCKET sock;
	CRingBuffer* sendBuf;
	CRingBuffer* recvBuf;
	DWORD dwLastMessageTime;

	BOOL bDeleted;
	BOOL bConnected;
	BOOL bConnectPending;
};

enum ERROR_TYPE
{
	SUCCESS = 0,
	TIMEOUT_NOTRECV,
	TIMEOUT_NOTRECV_LOGIN,
	NEED_TIMEOUT_USER,
	NEED_TIMEOUT_SESSION
};

//enum LOG_TYPE
//{
//	None = 0,
//	RecvTPS,
//	SendTPS,
//
//	ConnectWaitCount,
//	LoginWaitCount,
//
//	DisconnectFromServer,
//	ResponseFail
//};