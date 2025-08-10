//#pragma comment(lib,"ws2_32")
//#include <winsock2.h>
//#include <WS2tcpip.h>
//#include <process.h>
//#include <tchar.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include "CRingBuffer.h"
//#include "IOCPHeader.h"
//#include "ProcademyProfiler.h"
//
//// IO스레드에서 accept 진행
//// Send 스레드
//// Recv 스레드 생성
//
//SOCKET listen_sock;
//
//HANDLE _iocpHandle;
//HANDLE _acceptThreadHandle;
//HANDLE _workerThreadHandleArr[50];
//unsigned int _workerThreadID[50];
//
//unsigned int WINAPI AcceptThread(LPVOID arg);
//bool SetWSARecv(SOCKETINFO* ptr);
//bool SetWSASend(SOCKETINFO* ptr);
//
//int main(int argc, char* argv[])
//{
//	int retval;
//	//InitializeCriticalSection(&cs);
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
//	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++)
//	{
//		_workerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, 0, 0, &_workerThreadID[i]);
//		if (_workerThreadHandleArr[i] == NULL) return 1;
//	}
//
//	_acceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, 0, 0, 0);
//	if (_acceptThreadHandle == NULL) 
//		return 1;
//
//	while (1)
//	{
//		if (GetAsyncKeyState(VK_SPACE))
//		{
//			ProfileDataOutText("ProfileData_ZeroCopy_5000.txt");
//		}
//	}
//
//	WSACleanup();
//	//DeleteCriticalSection(&cs);
//	return 0;
//}
//
//unsigned int WINAPI AcceptThread(LPVOID arg)
//{
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
//		ptr->recvBuf = new CRingBuffer(BUFSIZE + 1);
//		ptr->sendBuf = new CRingBuffer(BUFSIZE + 1);
//
//		ZeroMemory(&ptr->recvOverlapped, sizeof(ptr->recvOverlapped));
//		ZeroMemory(&ptr->sendOverlapped, sizeof(ptr->sendOverlapped));
//		wsabuf.buf = ptr->recvBuf->GetRearBufferPtr();
//		wsabuf.len = BUFSIZE;
//		ptr->sock = client_sock;
//		InitializeCriticalSection(&ptr->_cs);
//		int recvRet = WSARecv(ptr->sock, &wsabuf, 1, &recvbytes, &flags, &ptr->recvOverlapped, NULL);
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
//	return 0;
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
//		OVERLAPPED* pOverlapped;
//		retval = GetQueuedCompletionStatus(_iocpHandle, &cbTransferred,
//			(PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);
//
//		SOCKADDR_IN clientaddr;
//		int addrlen = sizeof(clientaddr);
//		//getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);
//
//		
//		if (retval == 0 || cbTransferred == 0)
//		{
//			if (retval == 0)
//			{
//				DWORD temp1, temp2;
//				//WSAGetOverlappedResult(ptr->sock, &ptr->overlapped.overlappedVar,
//				//	&temp1, FALSE, &temp2);
//				err_display("WSAGetOverlappedResult()");
//			}
//			closesocket(ptr->sock);
//			//printf("[TCP 서버] 클라이언트 종료 : IP주소 = %s, 포트 번호 = %d\n",
//			//	inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
//			delete ptr;
//			continue;
//		}
//
//		if (&ptr->recvOverlapped == pOverlapped)
//		{
//			char tempBuffer[500];
//
//			EnterCriticalSection(&ptr->_cs);
//			ptr->recvBuf->MoveRear(cbTransferred);
//
//			int dequeueRet = ptr->recvBuf->Dequeue(tempBuffer, cbTransferred);
//			if (dequeueRet != cbTransferred)
//			{
//				DebugBreak();
//			}
//			//ptr->recvBuf[cbTransferred] = '\0';
//			int enqueueRet = ptr->sendBuf->Enqueue(tempBuffer, cbTransferred);
//			if (enqueueRet != cbTransferred)
//			{
//				DebugBreak();
//			}
//			//printf("[TCP / %s : %d] %s\n", inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50),
//			//	ntohs(clientaddr.sin_port), ptr->recvBuf);
//
//			SetWSARecv(ptr);
//			
//			//printf("\n[TCP WSARecv] IP주소 = %s, 포트 번호 = %d\n",
//			//	inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
//
//			// SEND
//			if (ptr->sendBuf->GetUseSize() > 0)
//			{
//				SetWSASend(ptr);
//			}
//			LeaveCriticalSection(&ptr->_cs);
//		}
//		else
//		{
//			EnterCriticalSection(&ptr->_cs);
//			ptr->sendBuf->MoveFront(cbTransferred);
//			if (ptr->sendBuf->GetUseSize() > 0)
//			{
//				SetWSASend(ptr);
//			}
//			else
//			{
//				// 1회 send
//			}
//
//			LeaveCriticalSection(&ptr->_cs);
//		}
//	}
//}
//
//bool SetWSASend(SOCKETINFO* ptr)
//{
//	int retval;
//	WSABUF sendWsa;
//	DWORD sendbytes;
//
//	sendWsa.buf = ptr->sendBuf->GetFrontBufferPtr();
//	sendWsa.len = ptr->sendBuf->DirectDequeueSize();
//	PRO_BEGIN("SEND_Zerocopy");
//	retval = WSASend(ptr->sock, &sendWsa, 1, &sendbytes,
//		0, &ptr->sendOverlapped, NULL);
//	PRO_END("SEND_Zerocopy");
//	//printf("[TCP WSASend] IP주소 = %s, 포트 번호 = %d | sendbytes : %d\n",
//	//	inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), sendbytes);
//
//	if (retval == SOCKET_ERROR)
//	{
//		if (WSAGetLastError() != WSA_IO_PENDING)
//		{
//			err_display("WSASend()");
//		}
//		else
//		{
//			//printf("[WSA_IO_PENDING] WSASend\n");
//		}
//		LeaveCriticalSection(&ptr->_cs);
//	}
//
//	return true;
//}
//
//bool SetWSARecv(SOCKETINFO* ptr)
//{
//	WSABUF recvWsa[2];
//	int recvRet;
//	DWORD flags = 0, recvbytes = 0;
//	ZeroMemory(&ptr->recvOverlapped, sizeof(ptr->recvOverlapped));
//
//	if (ptr->recvBuf->DirectEnqueueSize() < ptr->recvBuf->GetFreeSize())
//	{
//		// 두개로 나눠 받아야 함
//		recvWsa[0].buf = ptr->recvBuf->GetRearBufferPtr();
//		recvWsa[0].len = ptr->recvBuf->DirectEnqueueSize();
//
//		recvWsa[1].buf = ptr->recvBuf->GetArrPtr();
//		recvWsa[1].len = ptr->recvBuf->GetFreeSize() - ptr->recvBuf->DirectEnqueueSize();
//
//		recvRet = WSARecv(ptr->sock, recvWsa, 2, &recvbytes, &flags, &(ptr->recvOverlapped), NULL);
//	}
//	else
//	{
//		recvWsa[0].buf = ptr->recvBuf->GetRearBufferPtr();
//		recvWsa[0].len = ptr->recvBuf->GetFreeSize();
//
//		recvRet = WSARecv(ptr->sock, &recvWsa[0], 1, &recvbytes, &flags, &(ptr->recvOverlapped), NULL);
//	}
//
//	return true;
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