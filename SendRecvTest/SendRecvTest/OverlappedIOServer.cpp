//#pragma comment(lib,"ws2_32")
//#pragma comment(lib,"winmm.lib")
//#include <winsock2.h>
//#include <WS2tcpip.h>
//#include <process.h>
//#include <tchar.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include "IOCPHeader.h"
//#include "ProcademyProfiler.h"
//
//// IO스레드에서 accept 진행
//// Send 스레드
//// Recv 스레드 생성
//
//SRWLOCK _srwLock;
//
//SOCKET listen_sock;
//
//HANDLE _iocpHandle;
//HANDLE _controlThreadHandle;
//HANDLE _workerThreadHandleArr[50];
//unsigned int _workerThreadID[50];
//
//int main(int argc, char* argv[])
//{
//	timeBeginPeriod(1);
//
//	int retval;
//	InitializeCriticalSection(&cs);
//	InitializeSRWLock(&_srwLock);
//
//	// 윈속 초기화
//	WSADATA wsa;
//	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
//		return 1;
//
//	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
//	if (_iocpHandle == NULL) return 1;
//
//	// socket();
//	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
//	if (listen_sock == INVALID_SOCKET)
//		err_quit("socket()");
//
//	int optval = 0;
//	retval = setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));
//	if (retval == SOCKET_ERROR)
//		err_quit("SO_SNDBUF()");
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
//	//CPU 개수 확인
//	SYSTEM_INFO si;
//	GetSystemInfo(&si);
//
//	_controlThreadHandle = (HANDLE)_beginthreadex(NULL, 0, ControlThread, 0, 0, NULL);
//	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++)
//	{
//		_workerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, 0, 0, &_workerThreadID[i]);
//		if (_workerThreadHandleArr[i] == NULL) return 1;
//	}
//
//	// 데이터 통신에 사용할 변수
//	SOCKET client_sock;
//	SOCKADDR_IN clientaddr;
//	int addrlen;
//	DWORD recvbytes, flags;
//	char ipbuffer[50];
//
//	while (1)
//	{
//		//accept()
//		addrlen = sizeof(clientaddr);
//		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
//		if (client_sock == INVALID_SOCKET)
//		{
//			err_display("accept()");
//			continue;
//		}
//
//		printf("\n[TCP 서버] 클라이언트 접속 : IP주소 = %s, 포트 번호 = %d\n",
//			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
//
//		// 소켓을 IOCP에 등록
//		CreateIoCompletionPort((HANDLE)client_sock, _iocpHandle, (ULONG_PTR)&client_sock, 0);
//
//		// 비동기 입출력 시작
//		SOCKETINFO* ptr = new SOCKETINFO;
//		if (ptr == NULL) break;
//		WSABUF wsabuf;
//		flags = 0;
//
//		ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
//		wsabuf.buf = ptr->recvBuf;
//		wsabuf.len = BUFSIZE;
//		ptr->sock = client_sock;
//		ptr->overlapped.type = RECV;
//		int recvRet = WSARecv(ptr->sock, &wsabuf, 1, &recvbytes, &flags, &ptr->overlapped.overlappedVar, NULL);
//		if (recvRet == SOCKET_ERROR)
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				err_display("WSARecv()");
//				continue;
//			}
//		}
//		printf("\n[TCP Accept WSARecv] IP주소 = %s, 포트 번호 = %d | recvRet : %d\n",
//			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), recvRet);
//	}
//
//	WSACleanup();
//	DeleteCriticalSection(&cs);
//	timeEndPeriod(1);
//	return 0;
//}
//
//unsigned int WINAPI ControlThread(LPVOID arg)
//{
//	WCHAR ControlKey;
//	while (1)
//	{
//		ControlKey = _getwch();
//		if (ControlKey == L'q' || ControlKey == L'Q')
//		{
//			//------------------------------------------------
//			// 종료처리
//			//------------------------------------------------
//			ProfileDataOutText("OverlappedIOServer.txt");
//		}
//		Sleep(10);
//	}
//}
//
//unsigned int WINAPI WorkerThread(LPVOID arg)
//{
//	char ipbuffer[50];
//	int retval;
//
//	while (1)
//	{
//		WSABUF wsabuf;
//		DWORD cbTransferred, sendbytes, recvbytes;
//		SOCKET client_sock;
//		SOCKETINFO* ptr;
//		retval = GetQueuedCompletionStatus(_iocpHandle, &cbTransferred,
//			(PULONG_PTR)&client_sock, (LPOVERLAPPED*)&ptr, INFINITE);
//
//		SOCKADDR_IN clientaddr;
//		int addrlen = sizeof(clientaddr);
//		getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);
//
//		if (retval == 0 || cbTransferred == 0)
//		{
//			if (retval == 0)
//			{
//				DWORD temp1, temp2;
//				WSAGetOverlappedResult(ptr->sock, &ptr->overlapped.overlappedVar,
//					&temp1, FALSE, &temp2);
//				err_display("WSAGetOverlappedResult()");
//			}
//			closesocket(ptr->sock);
//			printf("[TCP 서버] 클라이언트 종료 : IP주소 = %s, 포트 번호 = %d\n",
//				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
//			delete ptr;
//			continue;
//		}
//
//		if (ptr->overlapped.type == RECV)
//		{
//			ptr->recvBuf[cbTransferred] = '\0';
//			memcpy(ptr->sendBuf, ptr->recvBuf, cbTransferred + 1);
//			printf("[TCP / %s : %d] %s\n", inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50),
//				ntohs(clientaddr.sin_port), ptr->recvBuf);
//
//			DWORD flags = 0;
//			ZeroMemory(&ptr->overlapped, sizeof(ptr->overlapped));
//			wsabuf.buf = ptr->recvBuf;
//			wsabuf.len = BUFSIZE;
//			int recvRet;
//			AcquireSRWLockExclusive(&_srwLock);
//			ptr->overlapped.type = RECV;
//			{
//				Profiler profiler("WSARecv_Return");
//				recvRet = WSARecv(ptr->sock, &wsabuf, 1, &recvbytes, &flags, &ptr->overlapped.overlappedVar, NULL);
//			}
//			ReleaseSRWLockExclusive(&_srwLock);
//			if (recvRet == SOCKET_ERROR)
//			{
//				if (WSAGetLastError() != WSA_IO_PENDING)
//				{
//					err_display("WSARecv()");
//					continue;
//				}
//			}
//			printf("\n[TCP WSARecv] IP주소 = %s, 포트 번호 = %d | recvRet : %d\n",
//				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), recvRet);
//
//			/*ptr->overlapped.type = SEND;
//			wsabuf.buf = ptr->sendBuf;
//			wsabuf.len = strlen(ptr->sendBuf);
//			{
//				Profiler profiler("WSASend_Return");
//				retval = WSASend(ptr->sock, &wsabuf, 1, &sendbytes,
//					0, &ptr->overlapped.overlappedVar, NULL);
//			}
//			printf("[TCP WSASend] IP주소 = %s, 포트 번호 = %d | sendbytes : %d\n",
//				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), sendbytes);
//
//			if (retval == SOCKET_ERROR)
//			{
//				if (WSAGetLastError() != WSA_IO_PENDING)
//				{
//					err_display("WSASend()");
//				}
//				else
//				{
//					printf("[WSA_IO_PENDING] WSASend\n");
//				}
//				continue;
//			}*/
//		}
//		else
//		{
//			ptr->overlapped.type = RECV;
//			DWORD cbTransferred, flag;
//			retval = WSAGetOverlappedResult(ptr->sock, &(ptr->overlapped.overlappedVar), &cbTransferred, FALSE, &flag);
//			if (retval == FALSE || cbTransferred == 0)
//			{
//				printf("[TCP 서버] 클라이언트 종료 : IP주소 = %s, 포트 번호 = %d\n",
//					inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
//				continue;
//			}
//
//			printf("[TCP WSASend Result] IP주소 = %s, 포트 번호 = %d | cbTransferred : %d\n",
//				inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), cbTransferred);
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