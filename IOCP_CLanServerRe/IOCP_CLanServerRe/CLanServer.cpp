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

void CLanServer::InitializeSessions(int maxConnection)
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