#pragma once
#define SERVERPORT	6000
#define BUFSIZE		512
#define PROTOCOL_SIZE 10

#pragma pack(1)
struct st_PACKET
{
	short shLen;
	LONGLONG llNum;
};

struct st_PACKET_HEADER
{
	DWORD dwSessionID;
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

int nTotalSockets = 0;
SRWLOCK _srwLock;

// 비동기 입출력 처리 함수
unsigned int WINAPI AcceptThread(LPVOID arg);
unsigned int WINAPI IOCPWorkerThread(LPVOID arg);
unsigned int WINAPI EchoThread(LPVOID arg);

// 오류 출력 함수
void err_quit(const char* msg);
void err_display(const char* msg);