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
	AcquireSRWLockExclusive(&_sessionMapLock);
	for (int i = 0; i < _imaxConnection; i++)
	{
		if (!_sessionArr[i].bSessionUsing)
		{
			ReleaseSRWLockExclusive(&_sessionMapLock);
			return i;
		}
	}

	ReleaseSRWLockExclusive(&_sessionMapLock);
	return -1;
}

// thread-safe 락 걸음
void CLanServer::FindSession(ULONG sessionID, st_Session** pSession)
{
	AcquireSRWLockExclusive(&_sessionMapLock);
	for (int i = 0; i < _imaxConnection; i++)
	{
		if (_sessionArr[i].ulSessionID == sessionID && _sessionArr[i].bSessionUsing)
		{
			*pSession = &_sessionArr[i];
			ReleaseSRWLockExclusive(&_sessionMapLock);
			return;
		}
	}
	ReleaseSRWLockExclusive(&_sessionMapLock);

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

	if (!Init(maxConnection))
		return false;

	printf("\n[TCP 서버] 시작\n");
}

unsigned int WINAPI CLanServer::AcceptThread(LPVOID arg)
{
	// static 선언해서 함수 호출을 위한 포인터
	CLanServer* thisPtr = (CLanServer*)arg;

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;

	while (1)
	{
		if (!_bServerEnabled)
		{
			break;
		}

		//accept()
		if (!(thisPtr->AcceptProc(thisPtr)))
		{
			continue;
		}
	}

	return 0;
}

bool CLanServer::AcceptProc(CLanServer* thisPtr)
{
	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	char ipbuffer[50];

	//accept()
	int addrlen = sizeof(clientaddr);
	client_sock = accept(_ListenSocket, (SOCKADDR*)&clientaddr, &addrlen);
	if (client_sock == INVALID_SOCKET)
	{
		err_display("accept()");
		return false;
	}

	// 비동기 입출력 시작
	int idx = FindUsableSessionIndex();
	if (idx == -1)
	{
		DebugBreak();
		return false;
	}

	st_Session* ptr = &_sessionArr[idx];

	// 수정이 필요함
	ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
	ptr->ulSessionID = _threadID++;
	ptr->dwIOCount = 0;
	ptr->bSendFlag = false;
	ptr->sock = client_sock;
	ptr->sendBuf->ClearBuffer();
	ptr->recvBuf->ClearBuffer();
	ptr->bSessionUsing = true;

	InterlockedIncrement((LONG*)&_iSessionCount);

	// 소켓을 IOCP에 등록
	CreateIoCompletionPort((HANDLE)client_sock, _NetIOCPHandle, (ULONG_PTR)ptr, 0);

	if (!SetWSARecv(ptr))
	{
		if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
		{
			// 연결 끊기
			ReleaseSession(ptr);
		}
	}

	return true;
}

unsigned int WINAPI CLanServer::IOCPWorkerThread(LPVOID arg)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;
	CLanServer* thisPtr = (CLanServer*)arg;

	while (1)
	{
		DWORD cbTransferred = 0;
		st_Session* ptr = NULL;
		OVERLAPPED* pOverlapped = NULL;

		retval = GetQueuedCompletionStatus(_NetIOCPHandle, &cbTransferred, (PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (retval == 0 && ptr == NULL && pOverlapped == NULL)
		{
			// 종료
			return 0;
		}

		if (retval == 0 && cbTransferred == 0)
		{
			if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
			{
				// 연결 끊기
				thisPtr->ReleaseSession(ptr);
			}
			continue;
		}

		if (pOverlapped == &ptr->recvOverlapped)
		{
			if (!thisPtr->RecvProc(ptr, cbTransferred))
			{
				if (InterlockedDecrement((DWORD*)&ptr->dwIOCount) == 0)
				{
					thisPtr->ReleaseSession(ptr);
					continue;
				}
			}

			if (!thisPtr->SetWSARecv(ptr))
			{
				if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
				{
					// 연결 끊기
					thisPtr->ReleaseSession(ptr);
					continue;
				}
			}
		}
		else
		{
			ptr->sendBuf->MoveFront(cbTransferred);
			int useSize = ptr->sendBuf->GetUseSize();
			if (useSize > 0)
			{
				if (!thisPtr->SetWSASend(ptr))
				{
					if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
					{
						// 연결 끊기
						thisPtr->ReleaseSession(ptr);
					}
				}
			}
			else
			{
				InterlockedExchange((DWORD*)&(ptr->bSendFlag), FALSE);
			}
		}

		if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
		{
			// 연결 끊기
			thisPtr->ReleaseSession(ptr);
		}
	}
}

unsigned int WINAPI CLanServer::EchoThread(LPVOID arg)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;
	CLanServer* thisPtr = (CLanServer*)arg;
	CPacket csPacket(PROTOCOL_MAX_SIZE);

	while (1)
	{
		WSABUF wsabuf;
		DWORD cbTransferred = 0;
		st_Session* ptr = NULL;
		OVERLAPPED* pOverlapped = NULL;
		st_PACKET_HEADER header;

		retval = GetQueuedCompletionStatus(_EchoIOCPHandle, &cbTransferred,
			(PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (cbTransferred == 0 && ptr == NULL && pOverlapped == NULL)
		{
			// 종료
			continue;
		}

		csPacket.Clear();

		EnterCriticalSection(&_echoBufferLock);
		if (_echoBuffer->GetUseSize() < sizeof(st_PACKET_HEADER) + PROTOCOL_NUMSIZE)
		{
			DebugBreak();
			LeaveCriticalSection(&_echoBufferLock);
			continue;
		}

		_echoBuffer->Dequeue((char*)&header, sizeof(st_PACKET_HEADER));
		_echoBuffer->Dequeue(csPacket.GetTailPtr(), PROTOCOL_NUMSIZE);
		LeaveCriticalSection(&_echoBufferLock);

		csPacket.MoveWritePos(PROTOCOL_NUMSIZE);
		thisPtr->FindSession(header.ulSessionID, &ptr);
		if (ptr == NULL)
		{
			continue;
		}

		// 세션은 찾았으니, 걔한테 SendPacket
		if (!thisPtr->SendPacket(header.ulSessionID, &csPacket))
		{
			if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
			{
				// 연결 끊기
				thisPtr->ReleaseSession(ptr);
				continue;
			}
		}
	}
}

void CLanServer::InitializeSessions(ULONG maxConnection)
{
	_sessionArr = (st_Session*)malloc(sizeof(st_Session) * maxConnection);
	_iSessionCount = 0;

	for (int i = 0; i < maxConnection; i++)
	{
		_sessionArr[i].bSessionUsing = false;
		_sessionArr[i].sendBuf = new CRingBuffer(15000);
		_sessionArr[i].recvBuf = new CRingBuffer(15000);
	}
}

bool CLanServer::Init(int maxConnection)
{
	_imaxConnection = maxConnection;
	InitializeSRWLock(&_sessionMapLock);
	InitializeCriticalSection(&_echoBufferLock);

	_echoBuffer = new CRingBuffer(100000);
	InitializeSessions(maxConnection);

	_NetIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_NetIOCPHandle == NULL) return false;

	_EchoIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_EchoIOCPHandle == NULL) return false;

	_acceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, &_acceptThreadID);
	if (_acceptThreadHandle == NULL)
		return false;

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2 - 1; i++)
	{
		_NetIOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, 0, &_NetIOCPWorkerThreadID[i]);
		if (_NetIOCPWorkerThreadHandleArr[i] == NULL)
			return false;
	}

	_EchoIOCPWorkerHandle = (HANDLE)_beginthreadex(NULL, 0, EchoThread, this, 0, &_EchoIOCPWorkerThreadID);
}

bool CLanServer::RecvProc(st_Session* ptr, DWORD cbTransferred)
{
	st_PACKET_HEADER header;
	st_NetHeader netHeader;
	CPacket csPacket(PROTOCOL_MAX_SIZE);

	ptr->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터 순회하며 Echo버퍼에 넣기
	while (1)
	{
		csPacket.Clear();

		int useSize = ptr->recvBuf->GetUseSize();
		if (useSize < sizeof(st_NetHeader))
		{
			break;
		}

		int peekRet = ptr->recvBuf->Peek((char*)&netHeader, sizeof(st_NetHeader));
		if (peekRet != sizeof(st_NetHeader))
		{
			DebugBreak();
			closesocket(ptr->sock);
			return false;
		}

		if (useSize < sizeof(st_NetHeader) + netHeader.shLen)
		{
			DebugBreak();
			closesocket(ptr->sock);
			return false;
		}

		ptr->recvBuf->MoveFront(sizeof(st_NetHeader));
		ptr->recvBuf->Dequeue((char*)csPacket.GetTailPtr(), netHeader.shLen);
		csPacket.MoveWritePos(netHeader.shLen);

		header.ulSessionID = ptr->ulSessionID;
		EnterCriticalSection(&_echoBufferLock);
		_echoBuffer->Enqueue((char*)&header, sizeof(st_PACKET_HEADER));
		int enqueueRet = _echoBuffer->Enqueue((char*)csPacket.GetBufferPtr(), netHeader.shLen);
		LeaveCriticalSection(&_echoBufferLock);
		if (enqueueRet != netHeader.shLen)
		{
			DebugBreak();
			closesocket(ptr->sock);
			return false;
		}

		// Post
		PostQueuedCompletionStatus(_EchoIOCPHandle, cbTransferred, (ULONG_PTR)&ptr, &(ptr->sendOverlapped));
	}
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
	char temp[PROTOCOL_MAX_SIZE + 1];

	st_Session* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;

	short shSize = cPacket->GetDataSize();
	cPacket->GetData(temp, shSize);

	st_NetHeader header;
	header.shLen = shSize;

	if (ptr->sendBuf->GetFreeSize() < sizeof(st_NetHeader) + shSize)
	{
		DebugBreak();
		Disconnect(sessionID);
		return false;
	}

	ptr->sendBuf->Enqueue((char*)&header, sizeof(st_NetHeader));
	ptr->sendBuf->Enqueue(temp, shSize);

	if (InterlockedExchange((LONG*)&(ptr->bSendFlag), TRUE) != TRUE)
	{
		if (!SetWSASend(ptr))
		{
			return false;
		}
	}

	InterlockedIncrement((unsigned int*)&_iSendMessageTPS);

	return true;
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

void CLanServer::ReleaseSession(st_Session* ptr)
{
	closesocket(ptr->sock);

	OnRelease(ptr->ulSessionID);
	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf->ClearBuffer();
	ptr->bSessionUsing = false;
	InterlockedDecrement((LONG*)&_iSessionCount);
}

void CLanServer::QuitServer()
{

}