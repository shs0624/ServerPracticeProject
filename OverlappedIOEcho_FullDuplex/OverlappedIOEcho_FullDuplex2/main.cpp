#pragma comment(lib,"ws2_32")
#include <winsock2.h>
#include <WS2tcpip.h>
#include <process.h>
#include <tchar.h>
#include <stdlib.h>
#include <stdio.h>
#include "header.h"

// IO스레드에서 accept 진행
// Send 스레드
// Recv 스레드 생성

SOCKET listen_sock;

HANDLE _recvThreadHandle;
HANDLE _sendThreadHandle;

unsigned int _sendThreadID;
unsigned int _recvThreadID;

int main(int argc, char* argv[])
{
	int retval;
	InitializeCriticalSection(&cs);

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket();
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
		err_quit("socket()");

	int optval = 0;
	retval = setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR)
		err_quit("SO_SNDBUF()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		err_quit("listen()");

	// 더미 이벤트 객체 생성
	WSAEVENT recvdummyEvent = WSACreateEvent();
	if (recvdummyEvent == WSA_INVALID_EVENT)
		err_quit("WSACreateEvent()");

	WSAEVENT senddummyEvent = WSACreateEvent();
	if (senddummyEvent == WSA_INVALID_EVENT)
		err_quit("WSACreateEvent()");


	RecvEventArray[nTotalSockets] = recvdummyEvent;
	SendEventArray[nTotalSockets++] = senddummyEvent;

	_sendThreadHandle = (HANDLE)_beginthreadex(NULL, 0, SendThread, (LPVOID)0, 0, &_sendThreadID);
	_recvThreadHandle = (HANDLE)_beginthreadex(NULL, 0, RecvThread, (LPVOID)0, 0, &_recvThreadID);

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	DWORD recvbytes, flags;
	char ipbuffer[50];

	while (1)
	{
		//accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			break;
		}

		printf("\n[TCP 서버] 클라이언트 접속 : IP주소 = %s, 포트 번호 = %d\n",
			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
		// 소켓 정보 추가
		if (AddSocketInfo(client_sock) == FALSE)
		{
			closesocket(client_sock);
			printf("\n[TCP 서버] 클라이언트 접속 끊김 : IP주소 = %s, 포트 번호 = %d\n",
				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
			continue;
		}

		// 비동기 입출력 시작
		SOCKETINFO* ptr = SocketInfoArray[nTotalSockets - 1];
		flags = 0;

		WSABUF wsabuf;
		wsabuf.buf = ptr->recvBuf;
		wsabuf.len = BUFSIZE;
		int recvRet = WSARecv(ptr->sock, &wsabuf, 1, &recvbytes, &flags, &ptr->recvoverlapped, NULL);
		if (recvRet == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSARecv()");
				RemoveSocketInfo(nTotalSockets - 1);
				continue;
			}
		}
		printf("\n[TCP WSARecv] IP주소 = %s, 포트 번호 = %d | recvRet : %d\n",
			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), recvRet);

		// 소켓 개수 변화를 알림
		WSASetEvent(RecvEventArray[0]);
		WSASetEvent(SendEventArray[0]);
	}

	WSACleanup();
	DeleteCriticalSection(&cs);
	return 0;
}

unsigned int WINAPI SendThread(LPVOID arg)
{
	char ipbuffer[50];
	int retval;

	while (1)
	{
		DWORD index = WSAWaitForMultipleEvents(nTotalSockets, SendEventArray,
			FALSE, WSA_INFINITE, FALSE);
		if (index == WSA_WAIT_FAILED) continue;
		index -= WSA_WAIT_EVENT_0;
		WSAResetEvent(SendEventArray[index]);
		if (index == 0) continue;

		// 클라이언트 정보 얻기
		SOCKETINFO* ptr = SocketInfoArray[index];
		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);

		// 데이터 보내기
		ZeroMemory(&ptr->sendoverlapped, sizeof(ptr->sendoverlapped));
		ptr->sendoverlapped.hEvent = SendEventArray[index];

		DWORD sendbytes;
		WSABUF wsabuf;
		wsabuf.buf = ptr->sendBuf;
		wsabuf.len = strlen(ptr->sendBuf);
		retval = WSASend(ptr->sock, &wsabuf, 1, &sendbytes,
			0, &ptr->sendoverlapped, NULL);
		printf("[TCP WSASend] IP주소 = %s, 포트 번호 = %d | sendbytes : %d\n",
			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), sendbytes);
		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSASend()");
			}
			else
			{
				printf("[WSA_IO_PENDING] WSASend\n");
			}

			WSAResetEvent(SendEventArray[index]);
			continue;
		}

		// Send 완료 후 이벤트 리셋
		WSAResetEvent(SendEventArray[index]);
	}
}

unsigned int WINAPI RecvThread(LPVOID arg)
{
	char ipbuffer[50];
	int retval;

	while (1)
	{
		DWORD index = WSAWaitForMultipleEvents(nTotalSockets, RecvEventArray,
			FALSE, WSA_INFINITE, FALSE);
		if (index == WSA_WAIT_FAILED) continue;
		index -= WSA_WAIT_EVENT_0;
		WSAResetEvent(RecvEventArray[index]);
		if (index == 0) continue;

		SOCKETINFO* ptr = SocketInfoArray[index];
		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);

		DWORD cbTransferred, flag;
		retval = WSAGetOverlappedResult(ptr->sock, &(ptr->recvoverlapped), &cbTransferred, FALSE, &flag);
		if (retval == FALSE || cbTransferred == 0)
		{
			RemoveSocketInfo(index);
			printf("[TCP 서버] 클라이언트 종료 : IP주소 = %s, 포트 번호 = %d\n",
				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
			continue;
		}

		//받은 데이터 출력
		ptr->recvBuf[cbTransferred] = '\0';
		memcpy(ptr->sendBuf, ptr->recvBuf, cbTransferred + 1);
		printf("[TCP / %s : %d] %s\n", inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50),
			ntohs(clientaddr.sin_port), ptr->recvBuf);

		// 데이터 받기
		ZeroMemory(&ptr->recvoverlapped, sizeof(ptr->recvoverlapped));
		ptr->recvoverlapped.hEvent = RecvEventArray[index];

		DWORD recvbytes;
		flag = 0;
		WSABUF wsabuf;
		wsabuf.buf = ptr->recvBuf;
		wsabuf.len = BUFSIZE;
		retval = WSARecv(ptr->sock, &wsabuf, 1, &recvbytes,
			&flag, &ptr->recvoverlapped, NULL);
		printf("[TCP WSARecv] IP주소 = %s, 포트 번호 = %d | retval : %d\n",
			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), recvbytes);

		SetEvent(SendEventArray[index]);
		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSARecv()");
			}
			continue;
		}
	}
}

// 소켓 정보 추가
BOOL AddSocketInfo(SOCKET sock)
{
	EnterCriticalSection(&cs);
	if (nTotalSockets >= WSA_MAXIMUM_WAIT_EVENTS) return FALSE;

	SOCKETINFO* ptr = new SOCKETINFO;
	if (ptr == NULL) return FALSE;

	WSAEVENT recvEvent = WSACreateEvent();
	if (recvEvent == WSA_INVALID_EVENT) return FALSE;

	WSAEVENT sendEvent = WSACreateEvent();
	if (sendEvent == WSA_INVALID_EVENT) return FALSE;

	ZeroMemory(&ptr->recvoverlapped, sizeof(ptr->recvoverlapped));
	ZeroMemory(&ptr->sendoverlapped, sizeof(ptr->sendoverlapped));
	ptr->recvoverlapped.hEvent = recvEvent;
	ptr->sendoverlapped.hEvent = sendEvent;
	ptr->sock = sock;
	SocketInfoArray[nTotalSockets] = ptr;
	RecvEventArray[nTotalSockets] = recvEvent;
	SendEventArray[nTotalSockets] = sendEvent;
	nTotalSockets++;

	LeaveCriticalSection(&cs);
	return TRUE;
}

void RemoveSocketInfo(int nIndex)
{
	EnterCriticalSection(&cs);

	SOCKETINFO* ptr = SocketInfoArray[nIndex];
	closesocket(ptr->sock);
	delete ptr;
	WSACloseEvent(RecvEventArray[nIndex]);
	WSACloseEvent(SendEventArray[nIndex]);

	if (nIndex != (nTotalSockets - 1))
	{
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets - 1];
		RecvEventArray[nIndex] = RecvEventArray[nTotalSockets - 1];
		SendEventArray[nIndex] = SendEventArray[nTotalSockets - 1];
	}
	--nTotalSockets;

	LeaveCriticalSection(&cs);
}

// 소켓 함수 오류 출력 후 종료
inline void err_quit(const char* msg)
{
	int err = WSAGetLastError();
	printf("[%s] TCP Error Number : %d\n", msg, err);
	exit(1);
}

inline void err_display(const char* msg)
{
	int err = WSAGetLastError();
	printf("[%s] TCP Error Number : %d\n", msg, err);
	return;
}