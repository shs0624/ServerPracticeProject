#pragma once
#define SERVERPORT	9000
#define BUFSIZE		512

enum SOCKTYPE
{
	SEND = 0,
	RECV
};

struct MYOVERLAPPED
{
	WSAOVERLAPPED overlappedVar;
	SOCKTYPE type;
};

struct SOCKETINFO
{
	MYOVERLAPPED overlapped;
	SOCKET sock;
	char sendBuf[BUFSIZE + 1];
	char recvBuf[BUFSIZE + 1];
};

int nTotalSockets = 0;
SOCKETINFO* SocketInfoArray[WSA_MAXIMUM_WAIT_EVENTS];
CRITICAL_SECTION cs;

// 비동기 입출력 처리 함수
unsigned int WINAPI WorkerThread(LPVOID arg);

// 오류 출력 함수
void err_quit(const char* msg);
void err_display(const char* msg);