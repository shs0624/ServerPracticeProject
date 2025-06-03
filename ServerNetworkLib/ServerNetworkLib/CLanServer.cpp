#pragma comment(lib,"ws2_32")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include <unordered_map>
#include "Debug.h"
#include "CLanServer.h"

//unsigned int WINAPI CLanServer::AcceptThread(LPVOID arg);
//unsigned int IOCPWorkerThread(LPVOID arg);

bool RecvProc(st_Session* ptr, DWORD cbTransferred);
bool SendProc(st_Session* ptr);

SOCKET listen_sock;

HANDLE _acceptThreadHandle;
HANDLE _iocpHandle;
HANDLE _iocpWorkerThreadHandleArr[50];

unsigned int _acceptThreadID;
unsigned int _iocpWorkerThreadID[50];

DWORD _threadID = 0;
CRITICAL_SECTION _sessionMapLock;
std::unordered_map<DWORD, st_Session*> _sessionMap;

bool CLanServer::Start(UCHAR* ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, int maxConnection)
{
	int retval;
	InitializeCriticalSection(&_sessionMapLock);

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrentThreads);
	if (_iocpHandle == NULL) return 1;

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
	serveraddr.sin_port = htons(port);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		err_quit("listen()");

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	_acceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, 0, 0, &_acceptThreadID);
	if (_acceptThreadHandle == NULL)
		return 1;

	for (int i = 0; i < workerCount; i++)
	{
		_iocpWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, 0, 0, &_iocpWorkerThreadID[i]);
		if (_iocpWorkerThreadHandleArr[i] == NULL)
			return 1;
	}

	printf("\n[TCP 서버] 시작\n");
}

unsigned int WINAPI CLanServer::AcceptThread(LPVOID arg)
{
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
			continue;
		}

		// 비동기 입출력 시작
		st_Session* ptr = new st_Session;
		if (ptr == NULL) break;

		// 소켓을 IOCP에 등록
		CreateIoCompletionPort((HANDLE)client_sock, _iocpHandle, (ULONG_PTR)ptr, 0);

		ZeroMemory(&ptr->recvOverlapped, sizeof(ptr->recvOverlapped));
		ZeroMemory(&ptr->sendOverlapped, sizeof(ptr->sendOverlapped));
		ptr->dwSessionID = _threadID++;
		ptr->dwIOCount = 1;
		ptr->bSendFlag = false;
		ptr->sock = client_sock;
		ptr->recvBuf = new CRingBuffer(15000);
		ptr->sendBuf = new CRingBuffer(15000);
		InitializeCriticalSection(&(ptr->CrtLock));

		EnterCriticalSection(&_sessionMapLock);
		_sessionMap.insert({ ptr->dwSessionID, ptr });
		LeaveCriticalSection(&_sessionMapLock);

		WSABUF wsabuf;
		flags = 0;

		wsabuf.buf = ptr->recvBuf->GetRearBufferPtr();
		wsabuf.len = ptr->recvBuf->GetFreeSize();
		int recvRet = WSARecv(ptr->sock, &wsabuf, 1, &recvbytes, &flags, &(ptr->recvOverlapped), NULL);
		if (recvRet == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSARecv()_Accept");
				continue;
			}
		}
	}

	return 0;
}

unsigned int IOCPWorkerThread(LPVOID arg)
{
	char ipbuffer[50];
	char tempBuffer[PROTOCOL_MAX_SIZE + 1];
	int retval;

	while (1)
	{
		WSABUF wsabuf;
		DWORD cbTransferred = 0, recvbytes;
		SOCKET client_sock;
		st_Session* ptr = NULL;
		OVERLAPPED* pOverlapped;
		retval = GetQueuedCompletionStatus(_iocpHandle, &cbTransferred,
			(PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (pOverlapped == 0 && cbTransferred == 0 && ptr == 0)
		{
			// 종료
			break;
		}

		if (retval == 0 || cbTransferred == 0)
		{
			if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
			{
				// 연결 끊기
				ReleaseSession(ptr);
			}
			continue;
		}

		if (&(ptr->recvOverlapped) == pOverlapped)
		{
			EnterCriticalSection(&(ptr->CrtLock));
			if (!RecvProc(ptr, cbTransferred))
			{
				if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
				{
					LeaveCriticalSection(&(ptr->CrtLock));
					// 연결 끊기
					ReleaseSession(ptr);
					continue;
				}
			}

			// WSARecv
			WSABUF recvWsa[2];
			int recvRet;
			DWORD flags = 0, recvbytes = 0;
			ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
			ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
			InterlockedIncrement((ULONGLONG*)&(ptr->dwIOCount));
			if (ptr->recvBuf->DirectEnqueueSize() < ptr->recvBuf->GetFreeSize())
			{
				// 두개로 나눠 받아야 함
				recvWsa[0].buf = ptr->recvBuf->GetRearBufferPtr();
				recvWsa[0].len = ptr->recvBuf->DirectEnqueueSize();

				recvWsa[1].buf = ptr->recvBuf->GetArrPtr();
				recvWsa[1].len = ptr->recvBuf->GetFreeSize() - ptr->recvBuf->DirectEnqueueSize();

				recvRet = WSARecv(ptr->sock, recvWsa, 2, &recvbytes, &flags, &(ptr->recvOverlapped), NULL);
			}
			else
			{
				recvWsa[0].buf = ptr->recvBuf->GetRearBufferPtr();
				recvWsa[0].len = ptr->recvBuf->GetFreeSize();

				recvRet = WSARecv(ptr->sock, &recvWsa[0], 1, &recvbytes, &flags, &(ptr->recvOverlapped), NULL);
			}
			LeaveCriticalSection(&(ptr->CrtLock));

			if (recvRet == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
					{
						// 연결 끊기
						ReleaseSession(ptr);
						continue;
					}
				}
			}
		}
		else
		{
			EnterCriticalSection(&(ptr->CrtLock));
			ptr->sendBuf->MoveFront(cbTransferred);
			int useSize = ptr->sendBuf->GetUseSize();
			if (useSize > 0)
			{
				SendProc(ptr);
			}
			else
			{
				InterlockedExchange((ULONGLONG*)&(ptr->bSendFlag), FALSE);
			}
			LeaveCriticalSection(&(ptr->CrtLock));
		}

		if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
		{
			// 연결 끊기
			ReleaseSession(ptr);
		}
	}

	return 1;
}

bool RecvProc(st_Session* ptr, DWORD cbTransferred)
{
	st_NetHeader header;
	char tempBuffer[PROTOCOL_MAX_SIZE + 1];
	CPacket* csPacket = new CPacket(PROTOCOL_MAX_SIZE);

	// 받은 데이터 카피
	ptr->recvBuf->MoveRear(cbTransferred);

	int sum = 0;
	// 받은 데이터를 전부 수신 링버퍼에서 빼고, 완성된 패킷들을 읽으며 Send링버퍼에 Enqueue
	while (1)
	{
		int useSize = ptr->recvBuf->GetUseSize();
		if (useSize < sizeof(st_NetHeader))
		{
			break;
		}

		int peekRet = ptr->recvBuf->Peek((char*) & header, sizeof(st_NetHeader));
		{
			DebugBreak();
			return false;
		}

		if (ptr->recvBuf->GetUseSize() < header.shLen + sizeof(st_NetHeader))
		{
			break;
		}

		ptr->recvBuf->MoveFront(sizeof(st_NetHeader));
		int dequeueRet = ptr->recvBuf->Dequeue(csPacket->GetBufferPtr(), header.shLen);
		if (dequeueRet != header.shLen + sizeof(st_NetHeader))
		{
			DebugBreak();
			return false;
		}

		csPacket->MoveWritePos(header.shLen);
		//OnRecv()
		csPacket->Clear();
	}

	return true;
}

bool SendProc(st_Session* ptr)
{
	int retval;
	DWORD sendbytes;

	InterlockedIncrement((ULONGLONG*)&(ptr->dwIOCount));
	int sendSize = ptr->sendBuf->GetUseSize();
	// WSASend
	if (ptr->sendBuf->DirectDequeueSize() < sendSize)
	{
		// 두개로 나눠 보내야함
		WSABUF sendWsa[2];
		sendWsa[0].buf = ptr->sendBuf->GetFrontBufferPtr();
		sendWsa[0].len = ptr->sendBuf->DirectDequeueSize();

		sendWsa[1].buf = ptr->sendBuf->GetArrPtr();
		sendWsa[1].len = sendSize - ptr->sendBuf->DirectDequeueSize();
		retval = WSASend(ptr->sock, sendWsa, 2, &sendbytes,
			0, &(ptr->sendOverlapped), NULL);
	}
	else
	{
		WSABUF sendWsa;
		sendWsa.buf = ptr->sendBuf->GetFrontBufferPtr();
		sendWsa.len = sendSize;
		retval = WSASend(ptr->sock, &sendWsa, 1, &sendbytes,
			0, &(ptr->sendOverlapped), NULL);
	}

	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			return false;
		}
	}

	return true;
}

void ReleaseSession(st_Session* ptr)
{
	EnterCriticalSection(&_sessionMapLock);
	_sessionMap.erase(ptr->dwSessionID);
	EnterCriticalSection(&(ptr->CrtLock));
	LeaveCriticalSection(&(ptr->CrtLock));
	LeaveCriticalSection(&_sessionMapLock);

	//*
	DeleteCriticalSection(&(ptr->CrtLock));
	closesocket(ptr->sock);

	delete(ptr->recvBuf);
	delete(ptr->sendBuf);
	delete ptr;
	//*/

	/*
	closesocket(ptr->sock);
	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf->ClearBuffer();
	EnterCriticalSection(&_poolLock);
	_sessionPool->Free(ptr);
	LeaveCriticalSection(&_poolLock);
	//*/
}