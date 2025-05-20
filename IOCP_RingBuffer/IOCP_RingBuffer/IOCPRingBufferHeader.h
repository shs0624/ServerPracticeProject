#pragma once
#define SERVERPORT	9000
#define BUFSIZE		512

struct SOCKETINFO
{
	OVERLAPPED sendOverlapped;
	OVERLAPPED recvOverlapped;
	SOCKET sock;
	CRingBuffer* sendBuf;
	CRingBuffer* recvBuf;
};

int nTotalSockets = 0;
CRITICAL_SECTION cs;

// 비동기 입출력 처리 함수
unsigned int WINAPI IOCPWorkerThread(LPVOID arg);

// 오류 출력 함수
void err_quit(const char* msg);
void err_display(const char* msg);