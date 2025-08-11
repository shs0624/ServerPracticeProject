#pragma once
#define SERVERPORT	9000
#define BUFSIZE		5012

enum SOCKTYPE
{
	SEND = 0,
	RECV
};

struct SOCKETINFO
{
	OVERLAPPED recvOverlapped;
	OVERLAPPED sendOverlapped;
	SOCKET sock;
	CRingBuffer* sendBuf;
	CRingBuffer* recvBuf;
	CRITICAL_SECTION _cs;
};

int nTotalSockets = 0;
SOCKETINFO* SocketInfoArray[WSA_MAXIMUM_WAIT_EVENTS];


// 비동기 입출력 처리 함수
unsigned int WINAPI WorkerThread(LPVOID arg);

// 오류 출력 함수
void err_quit(const char* msg);
void err_display(const char* msg);