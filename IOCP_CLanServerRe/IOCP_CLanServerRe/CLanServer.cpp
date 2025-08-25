#pragma comment(lib,"ws2_32")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include <crtdbg.h>
#include <minidumpapiset.h>
#include "CCrashDump.h"
#include "Debug.h"
#include "CLanServer.h"

procademy::CCrashDump cCrashDump;

CRITICAL_SECTION _echoBufferLock;
SRWLOCK _sessionMapLock;

SOCKET _ListenSocket;

HANDLE _acceptThreadHandle;
HANDLE _NetIOCPHandle;
HANDLE _NetIOCPWorkerThreadHandleArr[50];

HANDLE _EchoIOCPHandle;
HANDLE _EchoIOCPWorkerHandle;

DWORD _threadID = 0;

HANDLE _tpsThreadHandle;

CRingBuffer* _echoBuffer;

unsigned int _tpsThreadID;
unsigned int _acceptThreadID;
unsigned int _EchoIOCPWorkerThreadID;
unsigned int _NetIOCPWorkerThreadID[50];

bool _bServerEnabled = true;

// thread-safe 락 걸음
int CLanServer::FindUsableSessionIndex()
{
	AcquireSRWLockShared(&_sessionMapLock);
	for (int i = 0; i < 300; i++)
	{
		if (!_sessionArr[i].bSessionUsing)
			return i;
	}
	ReleaseSRWLockShared(&_sessionMapLock);

	return -1;
}

// thread-safe 락 걸음
void CLanServer::FindSession(ULONG sessionID, st_Session** pSession)
{
	AcquireSRWLockShared(&_sessionMapLock);
	for (int i = 0; i < 300; i++)
	{
		if (_sessionArr[i].ulSessionID == sessionID && _sessionArr[i].bSessionUsing)
		{
			*pSession = &_sessionArr[i];
			return;
		}
	}
	ReleaseSRWLockShared(&_sessionMapLock);

	*pSession = NULL;
	return;
}

bool CLanServer::Start(ULONG ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, int maxConnection)
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket();
	_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_ListenSocket == INVALID_SOCKET)
		err_quit("socket()");

	int optval = 0;
	retval = setsockopt(_ListenSocket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR)
		err_quit("SO_SNDBUF()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(_ListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// listen()
	retval = listen(_ListenSocket, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		err_quit("listen()");

	if (!Init())
		return false;

	printf("\n[TCP 서버] 시작\n");
}

void CLanServer::InitializeSessions(ULONG maxConnection)
{
	_sessionArr = (st_Session*)malloc(sizeof(st_Session) * 300);
	_iSessionCount = 0;

	for (int i = 0; i < 300; i++)
	{
		_sessionArr[i].bSessionUsing = false;
		_sessionArr[i].sendBuf = new CRingBuffer(15000);
		_sessionArr[i].recvBuf = new CRingBuffer(15000);
	}
}

bool CLanServer::Init()
{
	InitializeSRWLock(&_sessionMapLock);
	InitializeCriticalSection(&_echoBufferLock);

	_echoBuffer = new CRingBuffer(100000);
	InitializeSessions(300);

	_NetIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_NetIOCPHandle == NULL) return false;

	_EchoIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_EchoIOCPHandle == NULL) return false;

	_acceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, 0, 0, &_acceptThreadID);
	if (_acceptThreadHandle == NULL)
		return false;

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2 - 1; i++)
	{
		_NetIOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, 0, 0, &_NetIOCPWorkerThreadID[i]);
		if (_NetIOCPWorkerThreadHandleArr[i] == NULL)
			return false;
	}

	_EchoIOCPWorkerHandle = (HANDLE)_beginthreadex(NULL, 0, EchoThread, 0, 0, &_EchoIOCPWorkerThreadID);
}

bool CLanServer::Disconnect(ULONG sessionID)
{
	st_Session* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;

	closesocket(ptr->sock);
	return true;
}

bool CLanServer::SendPacket(ULONG sessionID, CPacket* cPacket)
{
	st_Session* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;


}

bool CLanServer::SetWSARecv(st_Session* ptr)
{
	// WSARecv
	WSABUF recvWsa[2];
	int recvRet;
	DWORD flags = 0, recvbytes = 0;
	ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
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

	if (recvRet == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			return false;
		}
	}

	return true;
}

bool CLanServer::SetWSASend(st_Session* ptr)
{
	int retval;
	DWORD sendbytes;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
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