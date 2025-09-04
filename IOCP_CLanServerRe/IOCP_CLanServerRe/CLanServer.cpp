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
#include "ProcademyProfiler.h"

procademy::CCrashDump cCrashDump;

CRITICAL_SECTION _echoBufferLock;
CRITICAL_SECTION _indexStackLock;

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
	int idx = -1;
	EnterCriticalSection(&_indexStackLock);
	if (_emptyIndexStack.count() != 0)
	{
		idx = _emptyIndexStack.top();
		_emptyIndexStack.pop();
	}
	LeaveCriticalSection(&_indexStackLock);
	return idx;
}

// thread-safe 락 걸음
void CLanServer::FindSession(ULONGLONG sessionID, st_Session** pSession)
{
	ULONGLONG idx = sessionID >> 48;
	if (sessionID == _sessionArr[idx].ulSessionID)
	{
		*pSession = &_sessionArr[idx];
	}
	else
	{
		*pSession = NULL;
	}

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

	ULONGLONG idx;
	// 비동기 입출력 시작
	{
		Profiler("FindSessionIdx");
		idx = FindUsableSessionIndex();
		if (idx == -1)
		{
			DebugBreak();
			return false;
		}
	}

	st_Session* ptr = &_sessionArr[idx];

	// 수정이 필요함
	ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
	ptr->bSessionAlive = true;
	ULONGLONG id = (_threadID++) & 0x0000ffffffffffff;
	ULONGLONG ulIdx = (idx << 48);
	ptr->ulSessionID = (ulIdx | id);
	ptr->dwIOCount = 0;
	ptr->bSendFlag = false;
	ptr->sock = client_sock;
	ptr->sendBuf.clear();
	ptr->recvBuf->ClearBuffer();
	
	InterlockedIncrement((LONG*)&_iAcceptTPS);
	InterlockedIncrement((LONG*)&_iSessionCount);

	OnAccept(ptr->ulSessionID);

	// 소켓을 IOCP에 등록
	CreateIoCompletionPort((HANDLE)client_sock, _NetIOCPHandle, (ULONG_PTR)ptr, 0);

	if (!SetWSARecv(ptr))
	{
		if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
		{
			// 연결 끊기
			ReleaseSession(ptr->ulSessionID);
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
				thisPtr->ReleaseSession(ptr->ulSessionID);
			}
			continue;
		}

		if (pOverlapped == &ptr->recvOverlapped)
		{
			{
				Profiler("RecvProc");
				if (!thisPtr->RecvProc(ptr, cbTransferred))
				{
					if (InterlockedDecrement((DWORD*)&ptr->dwIOCount) == 0)
					{
						thisPtr->ReleaseSession(ptr->ulSessionID);
						continue;
					}
				}
			}
			{
				Profiler("SetWSARecv");
				if (!thisPtr->SetWSARecv(ptr))
				{
					if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
					{
						// 연결 끊기
						thisPtr->ReleaseSession(ptr->ulSessionID);
						continue;
					}
				}
			}
		}
		else
		{
			EnterCriticalSection(&ptr->sendLock);
			int cnt = ptr->dwSendCount;
			for (int i = 0; i < cnt; i++)
			{
				ptr->sendBuf.pop_front();
			}

			ptr->dwSendCount -= cnt;
			if (ptr->dwSendCount < 0)
				DebugBreak();

			if (!ptr->sendBuf.empty())
			{
				if (!thisPtr->SetWSASend(ptr))
				{
					InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
					if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
					{
						// 연결 끊기
						thisPtr->ReleaseSession(ptr->ulSessionID);
					}
				}
			}
			else
			{
				InterlockedExchange((DWORD*)&(ptr->bSendFlag), FALSE);
			}
			LeaveCriticalSection(&ptr->sendLock);
		}

		if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
		{
			// 연결 끊기
			thisPtr->ReleaseSession(ptr->ulSessionID);
		}
	}
}

unsigned int WINAPI CLanServer::EchoThread(LPVOID arg)
{
	int retval;
	CLanServer* thisPtr = (CLanServer*)arg;

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

		RefCountPointer<CPacket> csPacket = RefCountPointer<CPacket>::MakeSharedPtr();
		(*csPacket)->Initialize(PROTOCOL_MAX_SIZE + 1, sizeof(st_NetHeader));

		EnterCriticalSection(&_echoBufferLock);
		if (_echoBuffer->GetUseSize() < sizeof(st_PACKET_HEADER) + PROTOCOL_NUMSIZE)
		{
			DebugBreak();
			LeaveCriticalSection(&_echoBufferLock);
			continue;
		}

		_echoBuffer->Dequeue((char*)&header, sizeof(st_PACKET_HEADER));
		_echoBuffer->Dequeue((*csPacket)->GetTailPtr(), PROTOCOL_NUMSIZE);
		LeaveCriticalSection(&_echoBufferLock);

		(*csPacket)->MoveWritePos(PROTOCOL_NUMSIZE);
		thisPtr->FindSession(header.ulSessionID, &ptr);
		if (ptr == NULL)
		{
			continue;
		}

		if (!ptr->bSessionAlive)
		{
			continue;
		}

		// 세션은 찾았으니, 걔한테 SendPacket
		thisPtr->SendPacket(header.ulSessionID, csPacket);
	}
}

void CLanServer::InitializeSessions(ULONG maxConnection)
{
	_sessionArr = (st_Session*)malloc(sizeof(st_Session) * maxConnection);
	_iSessionCount = 0;

	for (ULONGLONG i = 0; i < maxConnection; i++)
	{
		_sessionArr[i].bSessionAlive = false;
		_sessionArr[i].recvBuf = new CRingBuffer(15000);
		InitializeCriticalSection(&_sessionArr[i].sendLock);

		_emptyIndexStack.push(i);
	}
}

bool CLanServer::Init(int maxConnection)
{
	_imaxConnection = maxConnection;
	InitializeCriticalSection(&_echoBufferLock);
	InitializeCriticalSection(&_indexStackLock);

	_echoBuffer = new CRingBuffer(100000);
	InitializeSessions(maxConnection);

	_NetIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_NetIOCPHandle == NULL) return false;

	_EchoIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_EchoIOCPHandle == NULL) return false;

	_hTPSUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	_tpsThreadHandle = (HANDLE)_beginthreadex(NULL, 0, TPSThread, this, 0, &_acceptThreadID);
	if (_tpsThreadHandle == NULL)
		return false;

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
			Disconnect(ptr->ulSessionID);
			return false;
		}

		if (useSize < sizeof(st_NetHeader) + netHeader.shLen)
		{
			DebugBreak();
			Disconnect(ptr->ulSessionID);
			return false;
		}

		ptr->recvBuf->MoveFront(sizeof(st_NetHeader));
		ptr->recvBuf->Dequeue(csPacket.GetTailPtr(), netHeader.shLen);
		csPacket.MoveWritePos(netHeader.shLen);

		header.ulSessionID = ptr->ulSessionID;
		EnterCriticalSection(&_echoBufferLock);
		_echoBuffer->Enqueue((char*)&header, sizeof(st_PACKET_HEADER));
		int enqueueRet = _echoBuffer->Enqueue((char*)csPacket.GetHeadPtr(), netHeader.shLen);
		LeaveCriticalSection(&_echoBufferLock);
		if (enqueueRet != netHeader.shLen)
		{
			DebugBreak();
			Disconnect(ptr->ulSessionID);
			return false;
		}

		// Post
		PostQueuedCompletionStatus(_EchoIOCPHandle, cbTransferred, (ULONG_PTR)&ptr, &(ptr->sendOverlapped));

		InterlockedIncrement((LONG*)&_iRecvMessageTPS);
	}
}

bool CLanServer::Disconnect(ULONGLONG sessionID)
{
	st_Session* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;

	ptr->bSessionAlive = false;
	closesocket(ptr->sock);
	return true;
}

bool CLanServer::SendPacket(ULONGLONG sessionID, RefCountPointer<CPacket> cPacket)
{
	st_Session* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;

	short shSize = (*cPacket)->GetDataSize();
	st_NetHeader header;
	header.shLen = shSize;
	

	(*cPacket)->PushHeader((char*)&header, sizeof(st_NetHeader));

	EnterCriticalSection(&ptr->sendLock);
	ptr->sendBuf.push_back(cPacket);
	if (InterlockedExchange((LONG*)&(ptr->bSendFlag), TRUE) != TRUE)
	{
		if (!SetWSASend(ptr))
		{
			InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
			if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
			{
				// 연결 끊기
				ReleaseSession(ptr->ulSessionID);
			}
			LeaveCriticalSection(&ptr->sendLock);
			return false;
		}
	}
	LeaveCriticalSection(&ptr->sendLock);
	InterlockedIncrement((unsigned int*)&_iSendMessageTPS);

	return true;
}

bool CLanServer::SetWSARecv(st_Session* ptr)
{
	// WSARecv
	int recvRet, recvCount = 0;
	DWORD flags = 0, recvbytes = 0;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	WSABUF recvWsa[200];
	ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
	
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
	int retval, sendCount = 0;
	DWORD sendbytes;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	WSABUF sendWsa[200];

	int loopCnt = ptr->sendBuf.size();
	for (int i = 0; i < loopCnt; i++)
	{
		CPacket* cpacket = *(ptr->sendBuf[i]);
		sendWsa[i].buf = cpacket->GetBufferPtr();
		sendWsa[i].len = sizeof(st_NetHeader) + ((st_NetHeader*)sendWsa[i].buf)->shLen;
		sendCount++;
	}

	retval = WSASend(ptr->sock, sendWsa, sendCount, &sendbytes,
		0, &(ptr->sendOverlapped), NULL);
	ptr->dwSendCount = sendCount;

	if (retval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			printf("WSASend Fail! : %d\n", err);
			return false;
		}
	}

	return true;
}

bool CLanServer::SendLoginPacket(ULONGLONG ulSessionID, CPacket* cPacket)
{
	DWORD sendBytes, retval;
	st_Session* pSession = NULL;
	FindSession(ulSessionID, &pSession);
	if (pSession == NULL)
	{
		return false;
	}

	short shSize = (cPacket)->GetDataSize();
	st_NetHeader header;
	header.shLen = shSize;

	//EnterCriticalSection(&pSession->crtLock);
	(cPacket)->PushHeader((char*)&header, sizeof(st_NetHeader));

	WSABUF sendWsa;
	sendWsa.buf = cPacket->GetBufferPtr();
	sendWsa.len = cPacket->GetDataSize();

	int sendRet = WSASend(pSession->sock, &sendWsa, 1, &sendBytes, 0, &(pSession->sendOverlapped), NULL);
	if (sendRet == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
		{
			return false;
		}

		printf("Send SOCKET ERROR # ERRORNUM : %d\n", WSAGetLastError());
		return false;
	}
	//LeaveCriticalSection(&pSession->crtLock);

	InterlockedIncrement((unsigned int*)&_iSendMessageTPS);
	return true;
}

void CLanServer::ReleaseSession(ULONGLONG ulSessionID)
{
	st_Session* ptr;
	FindSession(ulSessionID, &ptr);
	if (ptr == NULL)
		return;

	if (!(ptr->bSessionAlive))
		return;

	ULONGLONG idx = (ulSessionID) >> 48;
	printf("Release Session IDX : %d\n", idx);
	OnRelease(ptr->ulSessionID);

	ptr->recvBuf->ClearBuffer();
	EnterCriticalSection(&ptr->sendLock);
	ptr->sendBuf.clear();
	LeaveCriticalSection(&ptr->sendLock);

	ptr->bSessionAlive = false;
	ptr->dwIOCount = 0;
	closesocket(ptr->sock);
	_emptyIndexStack.push(idx);

	// 인덱스를 아직 ID에 넣지 않음
	InterlockedDecrement((LONG*)&_iSessionCount);
}

void CLanServer::QuitServer()
{

}