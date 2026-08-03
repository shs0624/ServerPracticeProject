//#pragma comment(lib,"ws2_32")
//#include <WS2tcpip.h>
//#include <process.h>
//#include <Windows.h>
//#include "CRingBuffer.h"
//#include "IOCP_FullDuplex_New.h"
//
//HANDLE _iocpHandle;
//HANDLE _acceptThreadHandle;
//HANDLE _workerThreadHandleArr[50];
//
//SOCKET _listenSocket;
//
//unsigned int WINAPI AcceptThread(LPVOID arg);
//
//void SetWSARecv(SOCKETINFO* ptr);
//void SetWSASend(SOCKETINFO* ptr);
//
//int wmain()
//{
//	WSADATA wsa;
//	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
//	{
//		DebugBreak();
//		return 1;
//	}
//	
//	int retval;
//	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
//	if (_iocpHandle == NULL)
//		return 1;
//
//	_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
//	if (_listenSocket == INVALID_SOCKET)
//	{
//		DebugBreak();
//		return 1;
//	}
//
//	int optval = 0;
//	retval = setsockopt(_listenSocket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));
//	if (retval == SOCKET_ERROR)
//	{
//		DebugBreak();
//		return 1;
//	}
//
//	_acceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, 0, 0, 0);
//	if (_acceptThreadHandle == NULL)
//	{
//		DebugBreak();
//		return 1;
//	}
//
//	//bind
//	SOCKADDR_IN serverAddr;
//	ZeroMemory(&serverAddr, sizeof(serverAddr));
//	serverAddr.sin_family = AF_INET;
//	serverAddr.sin_port = htons(SERVERPORT);
//	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
//
//	retval = bind(_listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
//	if (retval == SOCKET_ERROR)
//	{
//		DebugBreak();
//		return 1;
//	}
//
//	retval = listen(_listenSocket, SOMAXCONN);
//	if(retval == SOCKET_ERROR)
//	{
//		DebugBreak();
//		return 1;
//	}
//
//	SYSTEM_INFO si;
//	GetSystemInfo(&si);
//
//	for (int i = 0; i < si.dwNumberOfProcessors * 2; i++)
//	{
//		_workerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, 0, 0, 0);
//		if(_workerThreadHandleArr[i] == NULL)
//		{
//			DebugBreak();
//			return 1;
//		}
//	}
//
//	while (1)
//	{
//
//	}
//
//	WSACleanup();
//	return 0;
//}
//
//unsigned int WINAPI AcceptThread(LPVOID arg)
//{
//	SOCKET _clientSocket;
//	SOCKADDR_IN clientAddr;
//	int addrlen; 
//	DWORD recvbytes, flag;
//	char ipbuffer[50];
//
//	while (1)
//	{
//		// accept
//		addrlen = sizeof(clientAddr);
//		_clientSocket = accept(_listenSocket, (SOCKADDR*)&clientAddr, &addrlen);
//		if (_clientSocket == INVALID_SOCKET)
//		{
//			int err = WSAGetLastError();
//			printf("[TCP ACCEPT ERROR] ERROR CODE : %d\n", err);
//			continue;
//		}
//
//		printf("\n[TCP 서버] 클라이언트 접속 : IP주소 = %s, 포트 번호 = %d\n",
//				inet_ntop(AF_INET, &(clientAddr.sin_addr), ipbuffer, 50), ntohs(clientAddr.sin_port));
//
//		// 구조체 생성, 초기 설정
//		SOCKETINFO* ptr = new SOCKETINFO;
//		ptr->recvBuf = new CRingBuffer(BUFSIZE + 1);
//		ptr->sendBuf = new CRingBuffer(BUFSIZE + 1);
//		ptr->sock = _clientSocket;
//		InitializeCriticalSection(&ptr->_cs);
//
//		// IOCP에 등록
//		CreateIoCompletionPort((HANDLE)_clientSocket, _iocpHandle, (ULONG_PTR)ptr, 0);
//
//		// Recv 설정
//		WSABUF wsabuf;
//		flag = 0;
//
//		ZeroMemory(&ptr->recvOverlapped, sizeof(ptr->recvOverlapped));
//		wsabuf.buf = ptr->recvBuf->GetRearBufferPtr();
//		wsabuf.len = ptr->recvBuf->DirectEnqueueSize();
//		int recvRet = WSARecv(ptr->sock, &wsabuf, 1, &recvbytes, &flag, &ptr->recvOverlapped, NULL);
//		if (recvRet == SOCKET_ERROR)
//		{
//			if (WSAGetLastError() != WSA_IO_PENDING)
//			{
//				// 연결 끊기
//				DebugBreak();
//				closesocket(ptr->sock);
//				continue;
//			}
//		}
//
//		//printf("\n[TCP Accept WSARecv] IP주소 = %s, 포트 번호 = %d | recvRet : %d\n",
//		//			inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), recvRet);
//	}
//}
//
//unsigned int WINAPI WorkerThread(LPVOID arg)
//{
//	int retval;
//	DWORD cbTransferred;
//	SOCKETINFO* ptr;
//	OVERLAPPED* pOverlap;
//
//	while (1)
//	{
//		retval = GetQueuedCompletionStatus(_iocpHandle, &cbTransferred,
//			(PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlap, INFINITE);
//
//		EnterCriticalSection(&ptr->_cs);
//		// 큐에서 꺼내질 못함 -> 서버가 닫혀야함
//		if (retval == 0)
//		{
//			closesocket(ptr->sock);
//			delete ptr;
//			DebugBreak();
//			return 1;
//		}
//
//		if (cbTransferred == 0)
//		{
//			closesocket(ptr->sock);
//			delete ptr;
//			continue;
//		}
//
//		// recv 완료통지
//		if (pOverlap == &(ptr->recvOverlapped))
//		{
//			char tempBuffer[BUFSIZE + 1];
//
//			// 받은 데이터를 recvBuf -> sendBuf로 인큐
//			ptr->recvBuf->MoveRear(cbTransferred);
//			int dequeueRet = ptr->recvBuf->Dequeue(tempBuffer, cbTransferred);
//			if (dequeueRet != cbTransferred)
//			{
//				DebugBreak();
//			}
//
//			int enqueueRet = ptr->sendBuf->Enqueue(tempBuffer, cbTransferred);
//			if (enqueueRet != cbTransferred)
//			{
//				//DebugBreak();
//			}
//
//			// 출력은 넣든가 말든가
//
//			// 다시 받기 걸고
//			SetWSARecv(ptr);
//
//			// 받은 클라에 전송
//			SetWSASend(ptr);
//		}
//		// send 완료통지
//		else
//		{
//			ptr->sendBuf->MoveFront(cbTransferred);
//		}
//		LeaveCriticalSection(&ptr->_cs);
//	}
//}
//
//void SetWSARecv(SOCKETINFO* ptr)
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
//	return;
//}
//
//// 받은 만큼 보내는 함수
//void SetWSASend(SOCKETINFO* ptr)
//{
//	int retval;
//	WSABUF sendWsa;
//	DWORD sendbytes;
//
//	sendWsa.buf = ptr->sendBuf->GetFrontBufferPtr();
//	sendWsa.len = ptr->sendBuf->DirectDequeueSize();
//	retval = WSASend(ptr->sock, &sendWsa, 1, &sendbytes,
//		0, &ptr->sendOverlapped, NULL);
//	//printf("[TCP WSASend] IP주소 = %s, 포트 번호 = %d | sendbytes : %d\n",
//	//	inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), sendbytes);
//
//	if (retval == SOCKET_ERROR)
//	{
//		int err = WSAGetLastError();
//		if (err != WSA_IO_PENDING)
//		{
//			DebugBreak();
//			return;
//		}
//		else
//		{
//			//printf("[WSA_IO_PENDING] WSASend\n");
//		}
//	}
//
//	return;
//}