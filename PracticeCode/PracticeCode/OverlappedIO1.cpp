#include "OverlappedTCPServer1.h"

int main(int argc, char* argv[])
{
	int retval;
	InitializeCriticalSection(&cs);

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket();
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
		err_quit("socket()");

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
	WSAEVENT hEvent = WSACreateEvent();
	if (hEvent == WSA_INVALID_EVENT)
		err_quit("WSACreateEvent()");

	EventArray[nTotalSockets++] = hEvent;

	HANDLE hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
	if (hThread == NULL) return 1;
	CloseHandle(hThread);

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
			printf("\n[TCP 서버] 클라이언트 접속 : IP주소 = %s, 포트 번호 = %d\n",
				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
			continue;
		}

		// 비동기 입출력 시작
		SOCKETINFO* ptr = SocketInfoArray[nTotalSockets - 1];
		flags = 0;
		retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes, &flags, &ptr->overlapped, NULL);
		if (retval == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSARecv()");
				RemoveSocketInfo(nTotalSockets - 1);
				continue;
			}
		}
		printf("\n[TCP WSARecv] IP주소 = %s, 포트 번호 = %d | retval : %d\n",
			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), retval);

		// 소켓 개수 변화를 알림
		WSASetEvent(EventArray[0]);
	}

	WSACleanup();
	DeleteCriticalSection(&cs);
	return 0;
}

DWORD WINAPI WorkerThread(LPVOID arg)
{
	char ipbuffer[50];
	int retval;

	while (1)
	{
		// 이벤트 객체 관찰
		DWORD index = WSAWaitForMultipleEvents(nTotalSockets, EventArray,
			FALSE, WSA_INFINITE, FALSE);
		if (index == WSA_WAIT_FAILED) continue;
		index -= WSA_WAIT_EVENT_0;
		WSAResetEvent(EventArray[index]);
		if (index == 0) continue;
		
		// 클라이언트 정보 얻기
		SOCKETINFO* ptr = SocketInfoArray[index];
		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);

		// 비동기 입출력 결과 확인
		DWORD cbTransferred, flags;
		retval = WSAGetOverlappedResult(ptr->sock, &ptr->overlapped, &cbTransferred,
			FALSE, &flags);
		if (retval == FALSE || cbTransferred == 0)
		{
			RemoveSocketInfo(index);
			printf("[TCP 서버] 클라이언트 종료 : IP주소 = %s, 포트 번호 = %d\n",
				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
			continue;
		}

		// 데이터 전송량 갱신
		if (ptr->recvbytes == 0)
		{
			ptr->recvbytes = cbTransferred;
			ptr->sendbytes = 0;
			//받은 데이터 출력
			ptr->buf[ptr->recvbytes] = '\0';
			printf("[TCP / %s : %d] %s\n", inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50),
				ntohs(clientaddr.sin_port), ptr->buf);
		}
		else
		{
			ptr->sendbytes += cbTransferred;
		}

		if (ptr->recvbytes > ptr->sendbytes)
		{
			// 데이터 보내기
			ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
			ptr->overlapped.hEvent = EventArray[index];
			ptr->wsabuf.buf = ptr->buf + ptr->sendbytes;
			ptr->wsabuf.len = ptr->recvbytes - ptr->sendbytes;

			DWORD sendbytes;
			flags = 0;
			retval = WSASend(ptr->sock, &ptr->wsabuf, 1, &sendbytes,
				0, &ptr->overlapped, NULL);
			printf("[TCP WSASend] IP주소 = %s, 포트 번호 = %d | sendbytes : %d\n",
				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), sendbytes);
			if (retval == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					err_display("WSASend()");
				}
				continue;
			}
		}
		else
		{
			ptr->recvbytes = 0;

			// 데이터 받기
			ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
			ptr->overlapped.hEvent = EventArray[index];
			ptr->wsabuf.buf = ptr->buf;
			ptr->wsabuf.len = BUFSIZE;

			DWORD recvbytes;
			flags = 0;
			retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes,
				&flags, &ptr->overlapped, NULL);
			printf("[TCP WSARecv] IP주소 = %s, 포트 번호 = %d | retval : %d\n",
				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), recvbytes);
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
}

// 소켓 정보 추가
BOOL AddSocketInfo(SOCKET sock)
{
	EnterCriticalSection(&cs);
	if (nTotalSockets >= WSA_MAXIMUM_WAIT_EVENTS) return FALSE;

	SOCKETINFO* ptr = new SOCKETINFO;
	if (ptr == NULL) return FALSE;

	WSAEVENT hEvent = WSACreateEvent();
	if (hEvent == WSA_INVALID_EVENT) return FALSE;

	ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
	ptr->overlapped.hEvent = hEvent;
	ptr->sock = sock;
	ptr->recvbytes = ptr->sendbytes = 0;
	ptr->wsabuf.buf = ptr->buf;
	ptr->wsabuf.len = BUFSIZE;
	SocketInfoArray[nTotalSockets] = ptr;
	EventArray[nTotalSockets] = hEvent;
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
	WSACloseEvent(EventArray[nIndex]);

	if (nIndex != (nTotalSockets - 1))
	{
		SocketInfoArray[nIndex] = SocketInfoArray[nTotalSockets - 1];
		EventArray[nIndex] = EventArray[nTotalSockets - 1];
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