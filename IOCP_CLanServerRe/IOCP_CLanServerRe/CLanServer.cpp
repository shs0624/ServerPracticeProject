#pragma comment(lib,"ws2_32")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include <crtdbg.h>
#include <minidumpapiset.h>
#include "CCrashDump.h"
#include "Debug.h"
#include "LockFreeStack_Re.h"
#include "CLanServer.h"
#include "ProcademyProfiler.h"
//#include "TLSMemoryPool.h"
#define IOCP_THREADCOUNT 5
#define LOGCOUNT 10000

//#define NETSERVER
#define LANSERVER

procademy::CCrashDump cCrashDump;
//TLSMemoryPoolManager<CPacket> _TLSPool(100, 3, 5);

enum logState
{
	ACCEPT,
	FREE
};

struct stLOG
{
	logState state;
	DWORD sockNum;
};

CRITICAL_SECTION _echoBufferLock;
CRITICAL_SECTION _indexStackLock;

SOCKET _ListenSocket;

HANDLE _acceptThreadHandle;
HANDLE _NetIOCPHandle;
HANDLE _NetIOCPWorkerThreadHandleArr[50];

HANDLE _EchoIOCPHandle;
HANDLE _EchoIOCPWorkerHandle;

DWORD _threadID = 0;
DWORD _logID = 0;
stLOG _logArr[10000];

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
	ptr->bSessionAlive = true;
	ULONGLONG id = (_threadID++) & 0x0000ffffffffffff;
	ULONGLONG ulIdx = (idx << 48);
	ptr->ulSessionID = (ulIdx | id);
	ptr->dwIOCount = 0;
	ptr->bSendFlag = false;
	ptr->_tempWSASendCheck = 0;
	ptr->sock = client_sock;
	ptr->sendBuf->Clear();
	ptr->recvBuf->ClearBuffer();
	ptr->bReleaseFlag = false;
	
	InterlockedIncrement((LONG*)&_iAcceptTPS);
	InterlockedIncrement((LONG*)&_iSessionCount);

	InterlockedIncrement(&ptr->dwIOCount);
	if (ptr->bReleaseFlag == 1)
	{
		DebugBreak();
		return false;
	}

	// 소켓을 IOCP에 등록
	CreateIoCompletionPort((HANDLE)client_sock, _NetIOCPHandle, (ULONG_PTR)ptr, 0);

	if (!OnAccept(ptr->ulSessionID))
	{
		DecrementIOCount(ptr);
		return false;
	}

	if (!SetWSARecv(ptr))
	{
		if (!DecrementIOCount(ptr))
		{
			DebugBreak();
			return false;
		}
	}

	if (!DecrementIOCount(ptr))
		return false;

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

		InterlockedIncrement(&ptr->dwIOCount);
		if (ptr->bReleaseFlag == TRUE)
		{
			continue;
		}		

		if (retval == 0 || cbTransferred == 0)
		{
			// 들어오면서 한 번, 완료통지에 관한 거 한 번
			thisPtr->DecrementIOCount(ptr);
			thisPtr->DecrementIOCount(ptr);

			continue;
		}

		if (pOverlapped == &ptr->recvOverlapped)
		{
			{
#ifdef LANSERVER
				if (!thisPtr->RecvProc(ptr, cbTransferred))
#endif
#ifdef NETSERVER
				if (!thisPtr->RecvProc_Net(ptr, cbTransferred))
#endif
				{
					if (!thisPtr->DecrementIOCount(ptr))
						continue;
				}
			}
			{
				//Profiler("SetWSARecv");
				if (!thisPtr->SetWSARecv(ptr))
				{
					if (!thisPtr->DecrementIOCount(ptr))
						continue;
				}
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
						continue;
				}
			}
			else
			{
				if (ptr->sendBuf->Empty())
				{
					// 내가 SendFlag를 바꿨다. 당연한거긴함
					if (InterlockedExchange((DWORD*)&(ptr->bSendFlag), FALSE) == TRUE)
					{
						if (!ptr->sendBuf->Empty())
						{
							if (InterlockedExchange((LONG*)&(ptr->bSendFlag), TRUE) != TRUE)
							{
								if (!thisPtr->SetWSASend(ptr))
								{
									InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);

									if (!thisPtr->DecrementIOCount(ptr))
										continue;
								}
							}
						}
					}
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
							continue;
					}
				}
			}
		}

		// 완료 통지에 대한 IO차감
		thisPtr->DecrementIOCount(ptr);

		// 여긴 세션 참조에 대한 IO차감
		thisPtr->DecrementIOCount(ptr);
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

		RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
#ifdef LANSERVER
		(*csPacket)->Initialize(PROTOCOL_MAX_SIZE + 1, sizeof(st_LanHeader));
#endif
		
#ifdef NETSERVER
		(*csPacket)->Initialize(PROTOCOL_MAX_SIZE + 1, sizeof(st_NetHeader));
#endif

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

		// 세션은 찾았으니, 걔한테 SendPacket
		thisPtr->SendPacket(header.ulSessionID, csPacket);
		csPacket.DecRefCount();
	}
}

void CLanServer::InitializeSessions(ULONG maxConnection)
{
	_sessionArr = (st_Session*)malloc(sizeof(st_Session) * maxConnection);
	_iSessionCount = 0;

	for (ULONGLONG i = 0; i < maxConnection; i++)
	{
		_sessionArr[i].bReleaseFlag = false;
		_sessionArr[i].bSessionAlive = false;
		_sessionArr[i].sendBuf = new LockFreeQueue<RefCountPointer>();
		_sessionArr[i].recvBuf = new CRingBuffer(15000);

		_emptyIndexStack.push(i);
	}
}

bool CLanServer::Init(int maxConnection)
{
	_imaxConnection = maxConnection;
	InitializeCriticalSection(&_echoBufferLock);
	InitializeCriticalSection(&_indexStackLock);

	_echoBuffer = new CRingBuffer(300000);
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

	_EchoIOCPWorkerHandle = (HANDLE)_beginthreadex(NULL, 0, EchoThread, this, 0, &_EchoIOCPWorkerThreadID);
}

bool CLanServer::RecvProc(st_Session* ptr, DWORD cbTransferred)
{
	st_PACKET_HEADER header;
	st_LanHeader netHeader;
	RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
	(*csPacket)->Initialize(PROTOCOL_MAX_SIZE, 0);

	ptr->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터 순회하며 Echo버퍼에 넣기
	while (1)
	{
		(*csPacket)->Clear();

		if (ptr->bReleaseFlag == TRUE)
			return false;

		int useSize = ptr->recvBuf->GetUseSize();
		if (useSize < sizeof(st_LanHeader))
		{
			break;
		}

		int peekRet = ptr->recvBuf->Peek((char*)&netHeader, sizeof(st_LanHeader));
		if (peekRet != sizeof(st_LanHeader))
		{
			DebugBreak();
			Disconnect(ptr->ulSessionID);
			return false;
		}

		if (ptr->recvBuf->GetUseSize() < sizeof(st_LanHeader) + netHeader.shLen)
		{
			DebugBreak();
			Disconnect(ptr->ulSessionID);
			return false;
		}

		ptr->recvBuf->MoveFront(sizeof(st_LanHeader));
		ptr->recvBuf->Dequeue((*csPacket)->GetTailPtr(), netHeader.shLen);
		(*csPacket)->MoveWritePos(netHeader.shLen);

		header.ulSessionID = ptr->ulSessionID;
		EnterCriticalSection(&_echoBufferLock);
		_echoBuffer->Enqueue((char*)&header, sizeof(st_PACKET_HEADER));
		int enqueueRet = _echoBuffer->Enqueue((char*)(*csPacket)->GetHeadPtr(), netHeader.shLen);
		LeaveCriticalSection(&_echoBufferLock);
		if (enqueueRet != netHeader.shLen)
		{
			DebugBreak();
 			Disconnect(ptr->ulSessionID);
			return false;
		}

		// Post
		PostQueuedCompletionStatus(_EchoIOCPHandle, cbTransferred, (ULONG_PTR)&ptr, NULL);
		InterlockedIncrement((LONG*)&_iRecvMessageTPS);
	}

	csPacket.DecRefCount();
	return true;
}

bool CLanServer::RecvProc_Net(st_Session* ptr, DWORD cbTransferred)
{
	st_PACKET_HEADER header;
	st_NetHeader netHeader;
	RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
	(*csPacket)->Initialize(PROTOCOL_MAX_SIZE, 0);

	ptr->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터 순회하며 Echo버퍼에 넣기
	while (1)
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

		short len = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->shLen;
		unsigned char RK = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->RandKey;
		if (useSize < sizeof(st_NetHeader) + len)
		{
			DebugBreak();
			Disconnect(ptr->ulSessionID);
			return false;
		}

		ptr->recvBuf->MoveFront(sizeof(st_NetHeader));
		ptr->recvBuf->Dequeue((*csPacket)->GetTailPtr(), len);
		(*csPacket)->MoveWritePos(len);

		// 디코드 필요
		(*csPacket)->Decode(FIXED_KEY, RK);
		unsigned char checkSum = (*csPacket)->GetCheckSum();
		if(checkSum != *((*csPacket)->GetCheckSumPtr()))
			DebugBreak();

		header.ulSessionID = ptr->ulSessionID;
		EnterCriticalSection(&_echoBufferLock);
		_echoBuffer->Enqueue((char*)&header, sizeof(st_PACKET_HEADER));
		int enqueueRet = _echoBuffer->Enqueue((char*)(*csPacket)->GetHeadPtr(), len);
		LeaveCriticalSection(&_echoBufferLock);
		if (enqueueRet != len)
		{
			DebugBreak();
			Disconnect(ptr->ulSessionID);
			return false;
		}

		// Post
		PostQueuedCompletionStatus(_EchoIOCPHandle, cbTransferred, (ULONG_PTR)&ptr, NULL);
		InterlockedIncrement((LONG*)&_iRecvMessageTPS);
	}

	csPacket.DecRefCount();
	return true;
}

bool CLanServer::DecrementIOCount(st_Session* ptr)
{
	if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
	{
		// 연결 끊기
		ReleaseSession(ptr->ulSessionID);
		return false;
	}
	
	return true;
}

bool CLanServer::Disconnect(ULONGLONG sessionID)
{
	st_Session* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;

	//CancelIoEx((HANDLE)ptr->sock, NULL);

	ReleaseSession(sessionID);
	ptr->bSessionAlive = false;

	return true;
}

void CLanServer::Encode(RefCountPointer& cPacket)
{
	//헤더를 넣은 cPacket이 들어온다는 가정 하에 짜자.
	unsigned char randKey = rand();

	//일단 체크섬을 빼고, 그 뒤 메세지를 이용해서 체크섬을 넣어야 한다.
	(*cPacket)->SetCheckSum();

	//그 후 체크섬을 포함해서 인코딩 공식을 사용. 체크섬 + 페이로드가 인코딩 대상
	(*cPacket)->Encode(FIXED_KEY, randKey);
}

bool CLanServer::SendPacket(ULONGLONG sessionID, RefCountPointer& cPacket)
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
		return false;
	}

	if (ptr->ulSessionID != sessionID)
	{
		DecrementIOCount(ptr);
		return false;
	}

	short shSize = (*cPacket)->GetDataSize();
#ifdef LANSERVER
	st_LanHeader header;
	header.shLen = shSize;
	
	(*cPacket)->PushHeader((char*)&header, sizeof(st_LanHeader));
#endif

#ifdef NETSERVER
	st_NetHeader netHeader;
	netHeader.FixedKey = FIXED_KEY;
	netHeader.RandKey = (unsigned char)rand();
	netHeader.shLen = shSize;

	(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
#endif

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

#ifdef SENDDEBUG
		LONG outCount = InterlockedDecrement(&ptr->_tempSendPacketCheck);
		if (ptr->_tempWSASendCheck >= 1)
			DebugBreak();
#endif
	}

	InterlockedIncrement((unsigned int*)&_iSendMessageTPS);
	DecrementIOCount(ptr);
	return true;
}

bool CLanServer::SetWSARecv(st_Session* ptr)
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

bool CLanServer::SetWSASend(st_Session* ptr)
{
	int retval, sendCount = 0;
	DWORD sendbytes;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	WSABUF sendWsa[200];

	RefCountPointer cpacket;
	int loopCnt = ptr->sendBuf->Size();
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
			if(err != 10054)
				printf("WSASend Fail! : %d\n", err);

			return false;
		}
	}

	return true;
}

bool CLanServer::SendLoginPacket(ULONGLONG ulSessionID)
{
	DWORD sendBytes, retval;
	st_Session* pSession = NULL;
	FindSession(ulSessionID, &pSession);
	if (pSession == NULL)
	{
		return false;
	}
	
	InterlockedIncrement(&pSession->dwIOCount);
	if (pSession->bReleaseFlag == 1)
	{
		DebugBreak();
		DecrementIOCount(pSession);
		return false;
	}

	if (pSession->ulSessionID != ulSessionID)
	{
		DecrementIOCount(pSession);
		return false;
	}


	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
#ifdef LANSERVER
	(*cPacket)->Initialize(PROTOCOL_MAX_SIZE + 1, sizeof(st_LanHeader));
#endif
#ifdef NETSERVER
	(*cPacket)->Initialize(PROTOCOL_MAX_SIZE + 1, sizeof(st_NetHeader));
#endif

	__int64 login = 0x7fffffffffffffff;
	*(*cPacket) << login;

	short shSize = (*cPacket)->GetDataSize();
#ifdef LANSERVER
	st_LanHeader header;
	header.shLen = shSize;

	(*cPacket)->PushHeader((char*)&header, sizeof(st_LanHeader));
#endif

#ifdef NETSERVER
	st_NetHeader netHeader;
	netHeader.FixedKey = FIXED_KEY;
	netHeader.RandKey = (unsigned char)rand();
	netHeader.shLen = shSize;

	(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
#endif

	pSession->sendBuf->Enqueue(cPacket);

	if (!SetWSASend(pSession))
	{
		// SetWSASend에 대한 IO 차감
		InterlockedExchange((LONG*)&(pSession->bSendFlag), FALSE);
		DecrementIOCount(pSession);

		// 얘는 리턴 전에 Login에 들어오며 올린 IOCount 차감
		DecrementIOCount(pSession);
		return false;
	}

	InterlockedIncrement((unsigned int*)&_iSendMessageTPS);
	DecrementIOCount(pSession);
	return true;
}

void CLanServer::ReleaseSession(ULONGLONG ulSessionID)
{
	st_Session* ptr;
	FindSession(ulSessionID, &ptr);
	if (ptr == NULL)
		return;

	// dwIOCount가 0이면서 Release가 False(0)이면 Release를 1로 변경
	if (_InterlockedCompareExchange64((LONGLONG*)&ptr->dwIOCount, 0x0000000100000000, 0x0000000000000000) != 0x0000000000000000)
		return;
	
	if (ptr->bReleaseFlag != TRUE)
		DebugBreak();

	ULONGLONG idx = (ulSessionID) >> 48;
	OnRelease(ptr->ulSessionID);

	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf->Clear();

	ptr->bSessionAlive = false;
	ptr->dwIOCount = 0;
	closesocket(ptr->sock);

	EnterCriticalSection(&_indexStackLock);
	_emptyIndexStack.push(idx);
	LeaveCriticalSection(&_indexStackLock);

	// 인덱스를 아직 ID에 넣지 않음
	InterlockedDecrement((LONG*)&_iSessionCount);
}

void CLanServer::QuitServer()
{

}