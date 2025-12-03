#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "LogManager.h"

procademy::CCrashDump cCrashDump;

SOCKET _ListenSocket;

HANDLE _acceptThreadHandle;
HANDLE _NetIOCPHandle;
HANDLE _NetIOCPWorkerThreadHandleArr[50];

DWORD _threadID = 0;
DWORD _logID = 0;

HANDLE _tpsThreadHandle;

unsigned int _tpsThreadID;
unsigned int _acceptThreadID;
unsigned int _EchoIOCPWorkerThreadID;
unsigned int _NetIOCPWorkerThreadID[50];

bool _bServerEnabled = true;

// 빌드에러 방지를 위한 정의
thread_local stChatLog CNetServer::_pLog;

// thread-safe 락프리 스택
int CNetServer::FindUsableSessionIndex()
{
	ULONGLONG idx = -1;
	while (true)
	{
		if (_emptyIndexStack->pop(&idx))
			break;

		Sleep(0);
	}
		
	return idx;
}

void CNetServer::FindSession(ULONGLONG sessionID, st_NetSession** pSession)
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
	retval = listen(_ListenSocket, SOMAXCONN_HINT(65535));
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

	// Accept스레드 로그 등록
	LogController::GetInstance()->RegisterLogStruct(&_pLog);

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
	_pLog._dwAcceptTPS++;
	_pLog._dwAcceptTotal++;

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

	st_NetSession* ptr = &_sessionArr[idx];

	// 수정이 필요함
	ZeroMemory(&(ptr->recvOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(OVERLAPPED));
	ULONGLONG id = (_threadID++) & 0x0000ffffffffffff;
	ULONGLONG ulIdx = (idx << 48);
	ptr->ulSessionID = (ulIdx | id);
	ptr->dwIOCount = 0;
	ptr->bReleaseFlag = false;
	ptr->bSendFlag = false;
	ptr->bCanceled = false;
	ptr->bDeleted = false;
	ptr->sock = client_sock;

	while (!ptr->sendBuf->Empty())
	{
		RefCountPointer cPacket;
		ptr->sendBuf->Dequeue(cPacket);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
	}
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

	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	while (1)
	{
		DWORD cbTransferred = 0;
		st_NetSession* ptr = NULL;
		OVERLAPPED* pOverlapped = NULL;

		retval = GetQueuedCompletionStatus(_NetIOCPHandle, &cbTransferred, (PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (retval == 0 && ptr == NULL && pOverlapped == NULL)
		{
			// 종료
			return 0;
		}

		// Release 작업 진행
		if (pOverlapped == &(thisPtr->_ReleaseOverlapped))
		{
			thisPtr->ReleaseSession(ptr->ulSessionID);
			continue;
		}

		InterlockedIncrement(&ptr->dwIOCount);
		if (ptr->bReleaseFlag == TRUE)
		{
			continue;
		}

		if (retval == 0 || cbTransferred == 0)
		{
			thisPtr->DecrementIOCount(ptr);
			thisPtr->DecrementIOCount(ptr);

			continue;
		}

		if (pOverlapped == &ptr->recvOverlapped)
		{
			if (!thisPtr->RecvProc_Net(ptr, cbTransferred))
			{
				if (!thisPtr->DecrementIOCount(ptr))
					continue;
			}
			
			if (!thisPtr->SetWSARecv(ptr))
			{
				if (!thisPtr->DecrementIOCount(ptr))
					continue;
			}
		}
		else
		{
			int cnt = ptr->dwSendCount;
			for (int i = 0; i < cnt; i++)
			{
				if (!ptr->cPacketArr[i].DecRefCount())
					_pLog._dwPacketPoolUse--;
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
				// 한 번 더 실제로 head가 비었는지 체크
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

void CNetServer::InitializeSessions(ULONG maxConnection)
{
	_sessionArr = (st_NetSession*)malloc(sizeof(st_NetSession) * maxConnection);
	_iSessionCount = 0;

	for (ULONGLONG i = 0; i < maxConnection; i++)
	{
		_sessionArr[i].bReleaseFlag = false;
		_sessionArr[i].sendBuf = new LockFreeQueue<RefCountPointer>();
		_sessionArr[i].recvBuf = new CRingBuffer(15000);

		_emptyIndexStack->push(i);
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

bool CNetServer::RecvProc_Net(st_NetSession* ptr, DWORD cbTransferred)
{
	st_NetHeader netHeader;
	ptr->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터 ChatServer에 전달
	while (1)
	{
		RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
		(*csPacket)->Initialize(sizeof(st_NetHeader));
		_pLog._dwPacketPoolUse++;

		short len;
		unsigned char RK;

		// csPacket 초기화 후 ptr->recvBuf에서 Dequeue
		{
			int useSize = ptr->recvBuf->GetUseSize();
			if (useSize < sizeof(st_NetHeader))
			{
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;
				break;
			}

			int peekRet = ptr->recvBuf->Peek((char*)(*csPacket)->GetBufferPtr(), sizeof(st_NetHeader));
			if (peekRet != sizeof(st_NetHeader))
			{
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;
				break;
			}

			len = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->shLen;
			RK = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->RandKey;
			if (useSize < sizeof(st_NetHeader) + len)
			{
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;
				break;
			}

			ptr->recvBuf->MoveFront(sizeof(st_NetHeader));
			ptr->recvBuf->Dequeue((*csPacket)->GetTailPtr(), len);

			(*csPacket)->MoveWritePos(len);
		}

		// 디코딩, 체크섬 검사
		if (!(*csPacket)->Decode(FIXED_KEY, RK))
		{
			Disconnect(ptr->ulSessionID);
			return false;
		}

		// netHeader만큼 이동시키고, OnRecv
		OnRecv(ptr->ulSessionID, csPacket);
		_pLog._dwRecvMessageTPS++;
	}

	return true;
}

bool CNetServer::DecrementIOCount(st_NetSession* ptr)
{
	LONG result = InterlockedDecrement((LONG*)&(ptr->dwIOCount));
	if (result == 0)
	{
		// 일단 CancelIO 한 번
		//CancelIoEx((HANDLE)ptr->sock, NULL);

		// 연결 끊기
		//ReleaseSession(ptr->ulSessionID);

		PostRelease(ptr);
		return false;
	}

	return true;
}

bool CNetServer::Disconnect(ULONGLONG sessionID)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;

	ptr->bCanceled = true;
	ptr->bDeleted = true;

	// @@TODO : 미흡한 처리를 보완해야함. CancelIO 이후 IOCP에 새 입출력이 들어갈 수도 있다.
	CancelIoEx((HANDLE)ptr->sock, NULL);

	// 이미 IO에 들어간 상태면 자연스럽게 Release를 타지 않을까?
	if (ptr->dwIOCount > 0)
		return false;

	ReleaseSession(sessionID);

	return true;
}

void CNetServer::PostRelease(st_NetSession* ptr)
{
	// 일부러 -1이 되게 Post
	//InterlockedIncrement(&ptr->dwIOCount);
	PostQueuedCompletionStatus(_NetIOCPHandle, 0, (ULONG_PTR)ptr, &_ReleaseOverlapped);
}

bool CNetServer::SendPacket_UniCast(ULONGLONG sessionID, RefCountPointer& cPacket, bool pushHeader)
{
	st_NetSession* ptr;
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

	if (pushHeader)
	{
		short shSize = (*cPacket)->GetDataSize();

		st_NetHeader netHeader;
		netHeader.FixedKey = PROGRAM_KEY;
		netHeader.RandKey = (unsigned char)rand() % 256;
		netHeader.shLen = shSize;

		(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
		(*cPacket)->Encode(FIXED_KEY, netHeader.RandKey);
	}

	//cPacket.IncRefCount();
	ptr->sendBuf->Enqueue(cPacket);

	if (InterlockedExchange((LONG*)&(ptr->bSendFlag), TRUE) != TRUE)
	{
		if (ptr->bCanceled)
		{
			DecrementIOCount(ptr);
			return false;
		}

		if (!SetWSASend(ptr))
		{
			InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);

			DecrementIOCount(ptr);
			DecrementIOCount(ptr);
			return false;
		}
	}

	_pLog._dwSendMessageTPS++;
	DecrementIOCount(ptr);
	return true;
}

bool CNetServer::SendPacket_MultiCast(ULONGLONG* sessionIDArr, WORD count, RefCountPointer& cPacket)
{
	// 메세지를 먼저 생성, 인코딩하기
	st_NetHeader netHeader;
	netHeader.FixedKey = PROGRAM_KEY;
	netHeader.RandKey = (unsigned char)rand() % 256;
	netHeader.shLen = (*cPacket)->GetDataSize();

	(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
	(*cPacket)->Encode(FIXED_KEY, netHeader.RandKey);

	// 그 후, 여러 세션에 하나의 메세지를 전송
	for (int i = 0; i < count; i++)
	{
		cPacket.IncRefCount();
		//@@TODO : 보내기 싫패하면 끊어야 할듯.
		if (!SendPacket_UniCast(sessionIDArr[i], cPacket, false))
		{
			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;
		}
	}

	// 자신 포함해서 다 보냈으니 1을 줄여야 짝이 맞는다.
	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;
	return true;
}

bool CNetServer::SetWSARecv(st_NetSession* ptr)
{
	// WSARecv
	int recvRet, recvCount = 0;
	DWORD flags = 0, recvbytes = 0;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	WSABUF recvWsa[200];
	ZeroMemory(&(ptr->recvOverlapped), sizeof(OVERLAPPED));
	//ZeroMemory(&(ptr->sendOverlapped), sizeof(OVERLAPPED));

	if (ptr->bCanceled)
		return false;

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

bool CNetServer::SetWSASend(st_NetSession* ptr)
{
	int retval, sendCount = 0;
	DWORD sendbytes;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	WSABUF sendWsa[200];

	if (ptr->bCanceled)
		return false;

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
			//printf("WSASend Fail! : %d\n", err);
			//DebugBreak();
			return false;
		}
	}

	return true;
}

void CNetServer::ReleaseSession(ULONGLONG ulSessionID)
{
	st_NetSession* ptr;
	FindSession(ulSessionID, &ptr);
	if (ptr == NULL)
		return;

	// dwIOCount가 0이면서 Release가 False(0)이면 Release를 1로 변경
	if (_InterlockedCompareExchange64((LONGLONG*)&ptr->dwIOCount, 0x0000000100000000, 0x0000000000000000) != 0x0000000000000000)
		return;

	ULONGLONG idx = (ulSessionID) >> 48;
	OnRelease(ptr->ulSessionID);

	ptr->recvBuf->ClearBuffer();
	while (!ptr->sendBuf->Empty())
	{
		RefCountPointer cPacket;
		ptr->sendBuf->Dequeue(cPacket);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
	}

	int cnt = ptr->dwSendCount;
	for (int i = 0; i < cnt; i++)
	{
		if (!ptr->cPacketArr[i].DecRefCount())
			_pLog._dwPacketPoolUse--;
	}

	ptr->dwSendCount = 0;
	ptr->dwIOCount = 0;
	closesocket(ptr->sock);

	// @@TODO : 락프리 스택 내부적으로 while돌리기
	while (true)
	{
		if (_emptyIndexStack->push(idx))
			break;

		Sleep(0);
	}

	// 인덱스를 아직 ID에 넣지 않음
	InterlockedDecrement((LONG*)&_iSessionCount);
}

void CNetServer::QuitServer()
{

}