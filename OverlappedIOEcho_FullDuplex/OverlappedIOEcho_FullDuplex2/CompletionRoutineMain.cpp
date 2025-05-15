//#include "CompletionRoutineheader.h"
//
//HANDLE _recvThreadHandle;
//HANDLE _sendThreadHandle;
//
//unsigned int _sendThreadID;
//unsigned int _recvThreadID;
//
//int main(int argc, char* argv[])
//{
//	int retval;
//
//	// 윈속 초기화
//	WSADATA wsa;
//	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
//		return 1;
//
//	// socket();
//	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
//	if (listen_sock == INVALID_SOCKET)
//		err_quit("socket()");
//
//	// bind()
//	SOCKADDR_IN serveraddr;
//	ZeroMemory(&serveraddr, sizeof(serveraddr));
//	serveraddr.sin_family = AF_INET;
//	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
//	serveraddr.sin_port = htons(SERVERPORT);
//	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
//	if (retval == SOCKET_ERROR)
//		err_quit("bind()");
//
//	// listen()
//	retval = listen(listen_sock, SOMAXCONN);
//	if (retval == SOCKET_ERROR)
//		err_quit("listen()");
//
//	//int DelayZeroOpt = 1;
//	//setsockopt(listen_sock, SOL_SOCKET, TCP_NODELAY, (const char*)&DelayZeroOpt, sizeof(DelayZeroOpt));
//
//	// 이벤트 객체 생성
//	hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
//	if (hReadEvent == NULL) return 1;
//	hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
//	if (hWriteEvent == NULL) return 1;
//
//	// 스레드 생성
//	HANDLE hThread = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
//	if (hThread == NULL) return 1;
//	CloseHandle(hThread);
//
//	while (1)
//	{
//		WaitForSingleObject(hReadEvent, INFINITE);
//		//accept()
//		client_sock = accept(listen_sock, NULL, NULL);
//		if (client_sock == INVALID_SOCKET)
//		{
//			err_display("accept()");
//			break;
//		}
//		SetEvent(hWriteEvent);
//	}
//
//	WSACleanup();
//	return 0;
//}
//
//DWORD WINAPI WorkerThread(LPVOID arg)
//{
//	int retval;
//	char ipbuffer[50];
//
//	while (1)
//	{
//		while (1)
//		{
//			//allertable wait
//			DWORD result = WaitForSingleObjectEx(hWriteEvent, INFINITE, TRUE);
//			if (result == WAIT_OBJECT_0) break;
//			if (result != WAIT_IO_COMPLETION) return 1;
//		}
//
//		// 클라이언트 정보 얻기
//		SOCKADDR_IN clientaddr;
//		int addrlen = sizeof(clientaddr);
//		getpeername(client_sock, (SOCKADDR*)&clientaddr, &addrlen);
//		printf("\n[TCP 서버] 클라이언트 접속 : IP주소 = %s, 포트 번호 = %d\n",
//			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
//
//		// 소켓 정보 구조체 할당과 초기화
//		SOCKETINFO* ptr = new SOCKETINFO;
//		if (ptr == NULL)
//		{
//			printf("[오류] 메모리 부족\n");
//			return 1;
//		}
//
//		ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
//		ptr->sock = client_sock;
//		SetEvent(hReadEvent);
//		ptr->recvbytes = ptr->sendbytes = 0;
//		ptr->wsabuf.buf = ptr->buf;
//		ptr->wsabuf.len = BUFSIZE;
//
//		// 비동기 입출력 시작
//		DWORD recvbytes;
//		DWORD flags = 0;
//		retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes,
//			&flags, &ptr->overlapped, CompletionRoutine);
//		if (retval == SOCKET_ERROR)
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				err_display("WSARecv()");
//				return 1;
//			}
//		}
//	}
//
//	return 0;
//}
//
//// 비동기 입출력 처리 함수
//void CALLBACK CompletionRoutine(
//	DWORD dwError, DWORD cbTransferred,
//	LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
//{
//	char ipbuffer[50];
//	int retval;
//
//	// 클라이언트 정보 얻기
//	SOCKETINFO* ptr = (SOCKETINFO*)lpOverlapped;
//	SOCKADDR_IN clientaddr;
//	int addrlen = sizeof(clientaddr);
//	getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);
//
//	// 비동기 입출력 결과 확인
//	if (dwError != 0 || cbTransferred == 0)
//	{
//		if (dwError != 0)
//			err_display(dwError);
//		closesocket(ptr->sock);
//		printf("[TCP 서버] 클라이언트 종료 : IP주소 = %s, 포트 번호 = %d\n",
//			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
//		delete ptr;
//		return;
//	}
//
//	// 데이터 전송량 갱신
//	if (ptr->recvbytes == 0)
//	{
//		ptr->recvbytes = cbTransferred;
//		ptr->sendbytes = 0;
//		// 받은 데이터 출력
//		ptr->buf[ptr->recvbytes] = '\0';
//		printf("[TCP / %s : %d] %s\n", inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50),
//			ntohs(clientaddr.sin_port), ptr->buf);
//	}
//	else
//	{
//		ptr->sendbytes += cbTransferred;
//	}
//
//	if (ptr->recvbytes > ptr->sendbytes)
//	{
//		// 데이터 보내기
//		ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
//		ptr->wsabuf.buf = ptr->buf + ptr->sendbytes;
//		ptr->wsabuf.len = ptr->recvbytes - ptr->sendbytes;
//
//		DWORD sendbytes;
//		retval = WSASend(ptr->sock, &ptr->wsabuf, 1, &sendbytes,
//			0, &ptr->overlapped, CompletionRoutine);
//		printf("[TCP WSASend] IP주소 = %s, 포트 번호 = %d | retval : %d\n",
//			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), sendbytes);
//		if (retval == SOCKET_ERROR)
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				err_display("WSASend()");
//				return;
//			}
//			else
//			{
//				printf("[WSA_IO_PENDING]\n");
//			}
//		}
//	}
//	else
//	{
//		ptr->recvbytes = 0;
//
//		// 데이터 받기
//		ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
//		ptr->wsabuf.buf = ptr->buf;
//		ptr->wsabuf.len = BUFSIZE;
//
//		DWORD recvbytes;
//		DWORD flags = 0;
//		retval = WSARecv(ptr->sock, &ptr->wsabuf, 1, &recvbytes,
//			&flags, &ptr->overlapped, CompletionRoutine);
//		printf("[TCP WSARecv] IP주소 = %s, 포트 번호 = %d | retval : %d\n",
//			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), retval);
//		if (retval == SOCKET_ERROR)
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				err_display("WSARecv()");
//				return;
//			}
//		}
//	}
//}
//
//// 소켓 함수 오류 출력 후 종료
//inline void err_quit(const char* msg)
//{
//	int err = WSAGetLastError();
//	printf("[%s] TCP Error Number : %d\n", msg, err);
//	exit(1);
//}
//
//inline void err_display(const char* msg)
//{
//	int err = WSAGetLastError();
//	printf("[%s] TCP Error Number : %d\n", msg, err);
//	return;
//}
//
//
//inline void err_display(int errcode)
//{
//	printf("TCP Error Number : %d\n", errcode);
//	return;
//}