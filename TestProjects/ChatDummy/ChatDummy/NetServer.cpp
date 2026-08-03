#pragma comment(lib,"ws2_32")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include <crtdbg.h>
#include <minidumpapiset.h>
#include "CCrashDump.h"
#include "DebugLog.h"
#include "NetServer.h"
#define IOCP_THREADCOUNT 5
#define LOGCOUNT 10000

procademy::CCrashDump cCrashDump;

SOCKET _ListenSocket;

HANDLE _acceptThreadHandle;
HANDLE _NetIOCPHandle;
HANDLE _NetIOCPWorkerThreadHandleArr[50];

HANDLE _EchoIOCPHandle;
HANDLE _EchoIOCPWorkerHandle;

DWORD _threadID = 0;
DWORD _logID = 0;

HANDLE _tpsThreadHandle;

unsigned int _tpsThreadID;
unsigned int _acceptThreadID;
unsigned int _EchoIOCPWorkerThreadID;
unsigned int _NetIOCPWorkerThreadID[50];

bool _bServerEnabled = true;

// thread-safe 락프리 스택
int CNetServer::FindUsableSessionIndex()
{
	ULONGLONG idx = -1;
	while (_emptyIndexStack.pop(&idx))
		break;

	return idx;
}

void CNetServer::FindSession(ULONGLONG sessionID, st_Session** pSession)
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

bool CNetServer::StartNetServer(ULONG ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, int maxConnection)
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

	LINGER lingerOpt = { 0,1 };
	int lingerRet = setsockopt(_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(LINGER));
	if (lingerRet == SOCKET_ERROR)
		err_quit("Linger()");

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

unsigned int WINAPI CNetServer::AcceptThread(LPVOID arg)
{
	// static 선언해서 함수 호출을 위한 포인터
	CNetServer* thisPtr = (CNetServer*)arg;

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

bool CNetServer::AcceptProc(CNetServer* thisPtr)
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
		//Profiler("FindSessionIdx");
		idx = FindUsableSessionIndex();
		if (idx == -1)
		{
			DebugBreak();
			return false;
		}
	}

	st_Session* ptr = &_sessionArr[idx];

	// 수정이 필요함
	ZeroMemory(&(ptr->recvOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(OVERLAPPED));
	ULONGLONG id = (_threadID++) & 0x0000ffffffffffff;
	ULONGLONG ulIdx = (idx << 48);
	ptr->ulSessionID = (ulIdx | id);
	ptr->dwIOCount = 0;
	ptr->bReleaseFlag = false;
	ptr->bSendFlag = false;
	ptr->sock = client_sock;
	ptr->sendBuf->Clear();
	ptr->recvBuf->ClearBuffer();

	InterlockedIncrement((LONG*)&_iAcceptTPS);
	InterlockedIncrement((LONG*)&_iSessionCount);

	InterlockedIncrement(&ptr->dwIOCount);
	if (ptr->bReleaseFlag == 1)
	{
		if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
		{
			// 연결 끊기
			ReleaseSession(ptr->ulSessionID);
		}
		return false;
	}

	if (!OnAccept(ptr->ulSessionID))
		return false;

	// 소켓을 IOCP에 등록
	CreateIoCompletionPort((HANDLE)client_sock, _NetIOCPHandle, (ULONG_PTR)ptr, 0);

	if (!SetWSARecv(ptr))
	{
		if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
		{
			// 연결 끊기
			thisPtr->ReleaseSession(ptr->ulSessionID);
			return false;
		}
	}

	if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
	{
		thisPtr->ReleaseSession(ptr->ulSessionID);
		return false;
	}

	return true;
}

unsigned int WINAPI CNetServer::IOCPWorkerThread(LPVOID arg)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;
	CNetServer* thisPtr = (CNetServer*)arg;

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

		/*if (((MYOVERLAPPED*)&pOverlapped)->ulSessionID != ptr->ulSessionID)
			continue;*/

		InterlockedIncrement(&ptr->dwIOCount);
		if (ptr->bReleaseFlag == TRUE)
		{
			if (!thisPtr->DecrementIOCount(ptr))
				goto DecRef;
		}

		if (retval == 0 || cbTransferred == 0)
		{
			if (!thisPtr->DecrementIOCount(ptr))
				goto DecRef;
		}

		if (pOverlapped == &ptr->recvOverlapped)
		{
			if (!thisPtr->RecvProc_Net(ptr, cbTransferred))
			{
				if (!thisPtr->DecrementIOCount(ptr))
					goto DecRef;
			}
			
			if (!thisPtr->SetWSARecv(ptr))
			{
				if (!thisPtr->DecrementIOCount(ptr))
					goto DecRef;
			}
		}
		else
		{
			int cnt = ptr->dwSendCount;
			for (int i = 0; i < cnt; i++)
			{
				ptr->cPacketArr[i].DecRefCount();
			}
			ptr->dwSendCount = 0;

			int size = ptr->sendBuf->Size();
			if (size > 0)
			{
				if (!thisPtr->SetWSASend(ptr))
				{
					InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
					if (!thisPtr->DecrementIOCount(ptr))
						goto DecRef;
				}
			}
			else
			{
				// 한 번 더 실제로 head가 비었는지 체크
				if (ptr->sendBuf->Empty())
				{
					InterlockedExchange((DWORD*)&(ptr->bSendFlag), FALSE);
				}
				else
				{
					while (ptr->sendBuf->Size() <= 0)
					{
						Sleep(0);
					}

					if (!thisPtr->SetWSASend(ptr))
					{
						InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
						if (!thisPtr->DecrementIOCount(ptr))
							goto DecRef;
					}
				}
			}
		}

		// 완료 통지에 대한 IO차감
		thisPtr->DecrementIOCount(ptr);
	DecRef:
		// 여긴 세션 참조에 대한 IO차감
		thisPtr->DecrementIOCount(ptr);
	}
}

void CNetServer::InitializeSessions(ULONG maxConnection)
{
	_sessionArr = (st_Session*)malloc(sizeof(st_Session) * maxConnection);
	_iSessionCount = 0;

	for (ULONGLONG i = 0; i < maxConnection; i++)
	{
		_sessionArr[i].bReleaseFlag = false;
		_sessionArr[i].sendBuf = new LockFreeQueue<RefCountPointer>();
		_sessionArr[i].recvBuf = new CRingBuffer(15000);

		_emptyIndexStack.push(i);
	}
}

bool CNetServer::Init(int maxConnection)
{
	_imaxConnection = maxConnection;
	InitializeSessions(maxConnection);

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	int concurrentThread = si.dwNumberOfProcessors - 4;
	if (concurrentThread <= 0)
		concurrentThread = si.dwNumberOfProcessors - 1;

	_NetIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrentThread);
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

	//IOCP_THREADCOUNT
	for (int i = 0; i < IOCP_THREADCOUNT; i++)
	{
		_NetIOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, 0, &_NetIOCPWorkerThreadID[i]);
		if (_NetIOCPWorkerThreadHandleArr[i] == NULL)
			return false;
	}
}

bool CNetServer::RecvProc_Net(st_Session* ptr, DWORD cbTransferred)
{
	st_NetHeader netHeader;
	ptr->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터 ChatServer에 전달
	while (1)
	{
		RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
		(*csPacket)->Initialize(PROTOCOL_MAX_SIZE, 0);

		short len;
		unsigned char RK;

		// csPacket 초기화 후 ptr->recvBuf에서 Dequeue
		{
			(*csPacket)->Clear();

			int useSize = ptr->recvBuf->GetUseSize();
			if (useSize < sizeof(st_NetHeader))
			{
				break;
			}

			int peekRet = ptr->recvBuf->Peek((char*)(*csPacket)->GetHeadPtr(), sizeof(st_NetHeader));
			if (peekRet != sizeof(st_NetHeader))
			{
				DebugBreak();
				Disconnect(ptr->ulSessionID);
				return false;
			}
			(*csPacket)->MoveReadPos(sizeof(st_NetHeader));

			len = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->shLen;
			RK = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->RandKey;
			if (useSize < sizeof(st_NetHeader) + len)
			{
				DebugBreak();
				Disconnect(ptr->ulSessionID);
				return false;
			}

			ptr->recvBuf->MoveFront(sizeof(st_NetHeader));
			ptr->recvBuf->Dequeue((*csPacket)->GetTailPtr(), len);
			(*csPacket)->MoveWritePos(len);
		}

		// 디코딩, 체크섬 검사
		if (!(*csPacket)->Decode(FIXED_KEY, RK))
			Disconnect(ptr->ulSessionID);

		// netHeader만큼 이동시키고, OnRecv
		(*csPacket)->MoveReadPos(sizeof(st_NetHeader));
		OnRecv(ptr->ulSessionID, csPacket);

		InterlockedIncrement((LONG*)&_iRecvMessageTPS);
	}

	return true;
}

bool CNetServer::DecrementIOCount(st_Session* ptr)
{
	if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
	{
		// 일단 CancelIO 한 번
		//CancelIoEx((HANDLE)ptr->sock, NULL);

		// 연결 끊기
		ReleaseSession(ptr->ulSessionID);
		return false;
	}

	return true;
}

bool CNetServer::Disconnect(ULONGLONG sessionID)
{
	st_Session* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;

	//CancelIoEx((HANDLE)ptr->sock, NULL);

	ReleaseSession(sessionID);

	return true;
}

bool CNetServer::SendPacket_UniCast(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	st_Session* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
	{
		return false;
	}

	InterlockedIncrement(&ptr->dwIOCount);
	if (ptr->bReleaseFlag == 1)
	{
		if (!DecrementIOCount(ptr))
			return false;
	}

	short shSize = (*cPacket)->GetDataSize();

	st_NetHeader netHeader;
	netHeader.FixedKey = FIXED_KEY;
	netHeader.RandKey = (unsigned char)rand();
	netHeader.shLen = shSize;

	(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
	(*cPacket)->Encode(FIXED_KEY);

	cPacket.IncRefCount();
	ptr->sendBuf->Enqueue(cPacket);

	if (InterlockedExchange((LONG*)&(ptr->bSendFlag), TRUE) != TRUE)
	{
		if (!SetWSASend(ptr))
		{
			InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);

			DecrementIOCount(ptr);
			DecrementIOCount(ptr);
			return false;
		}
	}

	InterlockedIncrement((unsigned int*)&_iSendMessageTPS);
	DecrementIOCount(ptr);
	return true;
}

bool CNetServer::SetWSARecv(st_Session* ptr)
{
	// WSARecv
	int recvRet, recvCount = 0;
	DWORD flags = 0, recvbytes = 0;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	WSABUF recvWsa[200];
	ZeroMemory(&(ptr->recvOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(OVERLAPPED));

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

bool CNetServer::SetWSASend(st_Session* ptr)
{
	int retval, sendCount = 0;
	DWORD sendbytes;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	WSABUF sendWsa[200];

	RefCountPointer cpacket;
	int loopCnt = ptr->sendBuf->Size();
	if (loopCnt >= 200)
		DebugBreak();

	for (int i = 0; i < loopCnt; i++)
	{
		(ptr->sendBuf->Dequeue(cpacket));
		ptr->cPacketArr[i] = cpacket;

		sendWsa[i].buf = (*cpacket)->GetBufferPtr();
		sendWsa[i].len = (*cpacket)->GetDataSize();
		sendCount++;
	}

	if (sendCount == 0)
	{
		return false;
	}

	ptr->dwSendCount = sendCount;
	retval = WSASend(ptr->sock, sendWsa, sendCount, &sendbytes,
		0, &(ptr->sendOverlapped), NULL);


	if (retval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			printf("WSASend Fail! : %d\n", err);
			//DebugBreak();
			return false;
		}
	}

	return true;
}

void CNetServer::ReleaseSession(ULONGLONG ulSessionID)
{
	st_Session* ptr;
	FindSession(ulSessionID, &ptr);
	if (ptr == NULL)
		return;

	// dwIOCount가 0이면서 Release가 False(0)이면 Release를 1로 변경
	if (_InterlockedCompareExchange64((LONGLONG*)&ptr->dwIOCount, 0x0000000100000000, 0x0000000000000000) != 0x0000000000000000)
		return;

	ULONGLONG idx = (ulSessionID) >> 48;
	OnRelease(ptr->ulSessionID);

	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf->Clear();

	ptr->dwIOCount = 0;
	closesocket(ptr->sock);
	_emptyIndexStack.push(idx);

	// 인덱스를 아직 ID에 넣지 않음
	InterlockedDecrement((LONG*)&_iSessionCount);
}

void CNetServer::QuitServer()
{

}