#pragma once
#pragma comment(lib,"ws2_32")
#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#define SERVERPORT	9000
#define BUFSIZE		512

enum OverrapedType
{
	SEND = 0,
	RECV
};

struct MYOVERLAPPED
{
	WSAOVERLAPPED overlapped;
};

struct SOCKETINFO
{
	WSAOVERLAPPED overlapped;
	SOCKET sock;
	char buf[BUFSIZE + 1];
	int recvbytes;
	int sendbytes;
	WSABUF wsabuf;
};

SOCKET client_sock;
HANDLE hReadEvent, hWriteEvent;

// 비동기 입출력 처리 함수
DWORD WINAPI WorkerThread(LPVOID arg);
void CALLBACK CompletionRoutine(
	DWORD dwError, DWORD cbTransferred,
	LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags
);

// 오류 출력 함수
void err_quit(const char* msg);
void err_display(const char* msg);
void err_display(int errcode);