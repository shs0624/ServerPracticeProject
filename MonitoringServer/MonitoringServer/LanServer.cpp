#pragma once
#include "Includes.h"
#include "LanServer.h"
#include "LogManager.h"

// 빌드에러 방지를 위한 정의
thread_local stChatLog CLanServer::_pLog;

// thread-safe 락프리 스택
int CLanServer::FindUsableSessionIndex()
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

void CLanServer::FindSession(ULONGLONG sessionID, st_NetSession** pSession)
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

bool CLanServer::StartLanServer(ULONG ip, LONG port, int concurrentThreads, bool bNagleEnabled, int maxConnection, unsigned char programKey, unsigned char fixedKey)
{
	int retval;

	_FixedKey = fixedKey;
	_ProgramKey = programKey;

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
	serveraddr.sin_port = htons(port);
	retval = ::bind(_ListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
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

unsigned int WINAPI CLanServer::AcceptThread(LPVOID arg)
{
	// static 선언해서 함수 호출을 위한 포인터
	CLanServer* thisPtr = (CLanServer*)arg;

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;

	// Accept스레드 로그 등록
	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	while (1)
	{
		if (!thisPtr->_bServerEnabled)
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

	st_NetSession* ptr = &_sessionArr[idx];

	// 다른 곳에서 Send후 Dec로 해제되는 걸 막기위해 먼저 Inc
	ptr->dwIOCount = 0;
	if (InterlockedIncrement(&ptr->dwIOCount) != 1)
		DebugBreak();

	while (ptr->sendBuf->Size() > 0)
	{
		RefCountPointer cPacket;
		ptr->sendBuf->Dequeue(cPacket);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
	}
	ptr->recvBuf->ClearBuffer();

	// 수정이 필요함
	ZeroMemory(&(ptr->recvOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(OVERLAPPED));
	ULONGLONG id = (_threadID++) & 0x0000ffffffffffff;
	ULONGLONG ulIdx = (idx << 48);
	ptr->dwSendCount = 0;
	ptr->clientAddr = clientaddr;
	ptr->ulSessionID = (ulIdx | id);
	ptr->bReleaseFlag = false;
	ptr->bSendFlag = false;
	ptr->bCanceled = false;
	ptr->bDeleted = false;
	ptr->sock = client_sock;

	InterlockedIncrement((LONG*)&_iAcceptTPS);
	InterlockedIncrement((LONG*)&_iSessionCount);

	if (!OnAccept(ptr->ulSessionID, clientaddr))
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

unsigned int WINAPI CLanServer::IOCPWorkerThread(LPVOID arg)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;
	CLanServer* thisPtr = (CLanServer*)arg;

	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	while (1)
	{
		DWORD cbTransferred = 0;
		st_NetSession* ptr = NULL;
		OVERLAPPED* pOverlapped = NULL;

		retval = GetQueuedCompletionStatus(thisPtr->_NetIOCPHandle, &cbTransferred, (PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

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
				if (ptr->sendBuf->Empty())
				{
					if (InterlockedExchange((DWORD*)&(ptr->bSendFlag), FALSE) == TRUE)
					{
						if (ptr->sendBuf->Size() > 0)
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
		if (!thisPtr->DecrementIOCount(ptr))
			continue;
		// 여긴 세션 참조에 대한 IO차감
		if (!thisPtr->DecrementIOCount(ptr))
			continue;
	}
}

void CLanServer::InitializeSessions(ULONG maxConnection)
{
	_emptyIndexStack = new LockFreeStack<ULONGLONG>();

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

bool CLanServer::Init(int maxConnection)
{
	_imaxConnection = maxConnection;
	InitializeSessions(maxConnection);

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	int concurrentThread = si.dwNumberOfProcessors - 2;
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

bool CLanServer::RecvProc_Net(st_NetSession* ptr, DWORD cbTransferred)
{
	st_LanHeader netHeader;
	ptr->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터 ChatServer에 전달
	while (1)
	{
		RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
		(*csPacket)->Initialize(sizeof(st_LanHeader));
		_pLog._dwPacketPoolUse++;

		short len;
		unsigned char RK;

		// csPacket 초기화 후 ptr->recvBuf에서 Dequeue
		{
			int useSize = ptr->recvBuf->GetUseSize();
			if (useSize < sizeof(st_LanHeader))
			{
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;
				break;
			}

			int peekRet = ptr->recvBuf->Peek((char*)(*csPacket)->GetBufferPtr(), sizeof(st_LanHeader));
			if (peekRet != sizeof(st_LanHeader))
			{
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;
				break;
			}

			len = ((st_LanHeader*)((*csPacket)->GetBufferPtr()))->shLen;
			if (len < 0 || len > PROTOCOL_MAX_SIZE) {
				Disconnect(ptr->ulSessionID);
				return false;
			}

			RK = ((st_LanHeader*)((*csPacket)->GetBufferPtr()))->RandKey;
			if (useSize < sizeof(st_LanHeader) + len)
			{
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;
				break;
			}

			ptr->recvBuf->MoveFront(sizeof(st_LanHeader));
			ptr->recvBuf->Dequeue((*csPacket)->GetTailPtr(), len);

			(*csPacket)->MoveWritePos(len);
		}

		// 디코딩, 체크섬 검사
		if (!(*csPacket)->Decode(_FixedKey, RK))
		{
			Disconnect(ptr->ulSessionID);
			return false;
		}

		// netHeader만큼 이동시키고, OnRecv
		OnRecv(ptr->ulSessionID, csPacket);
	}

	return true;
}

bool CLanServer::DecrementIOCount(st_NetSession* ptr)
{
	LONG result = InterlockedDecrement((LONG*)&(ptr->dwIOCount));
	if (result == 0)
	{
		PostRelease(ptr);
		return false;
	}

	return true;
}

bool CLanServer::Disconnect(ULONGLONG sessionID)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;

	if (sessionID != ptr->ulSessionID)
	{
		return false;
	}

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

bool CLanServer::GetClientAddr(ULONGLONG sessionID, WCHAR* buffer, int len)
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
		DecrementIOCount(ptr);
		return false;
	}

	if (sessionID != ptr->ulSessionID)
	{
		DecrementIOCount(ptr);
		return false;
	}

	if (InetNtop(AF_INET, &ptr->clientAddr.sin_addr, buffer, len)) {
		DecrementIOCount(ptr);
		return true;
	}

	DecrementIOCount(ptr);
	return false;

}

void CLanServer::PostRelease(st_NetSession* ptr)
{
	// 일부러 -1이 되게 Post
	//InterlockedIncrement(&ptr->dwIOCount);
	PostQueuedCompletionStatus(_NetIOCPHandle, 0, (ULONG_PTR)ptr, &_ReleaseOverlapped);
}

bool CLanServer::SendPacket_UniCast(ULONGLONG sessionID, RefCountPointer& cPacket, bool pushHeader)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		return false;
	}

	InterlockedIncrement(&ptr->dwIOCount);
	if (ptr->bReleaseFlag == 1)
	{
		DecrementIOCount(ptr);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		return false;
	}

	if (sessionID != ptr->ulSessionID)
	{
		DecrementIOCount(ptr);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		return false;
	}

	if (pushHeader)
	{
		short shSize = (*cPacket)->GetDataSize();

		st_LanHeader netHeader;
		netHeader.FixedKey = _ProgramKey;
		netHeader.RandKey = (unsigned char)rand() % 256;
		netHeader.shLen = shSize;

		(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_LanHeader));
		(*cPacket)->Encode(_FixedKey, netHeader.RandKey);
	}

	//cPacket.IncRefCount();
	ptr->sendBuf->Enqueue(cPacket);

	if (InterlockedExchange((LONG*)&(ptr->bSendFlag), TRUE) != TRUE)
	{
		if (ptr->bCanceled)
		{
			InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);

			DecrementIOCount(ptr);
			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;

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

	DecrementIOCount(ptr);
	return true;
}

bool CLanServer::SendPacket_MultiCast(ULONGLONG* sessionIDArr, WORD count, RefCountPointer& cPacket)
{
	// 메세지를 먼저 생성, 인코딩하기
	st_LanHeader netHeader;
	netHeader.FixedKey = _ProgramKey;
	netHeader.RandKey = (unsigned char)rand() % 256;
	netHeader.shLen = (*cPacket)->GetDataSize();

	(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_LanHeader));
	(*cPacket)->Encode(_FixedKey, netHeader.RandKey);

	// 그 후, 여러 세션에 하나의 메세지를 전송
	for (int i = 0; i < count; i++)
	{
		cPacket.IncRefCount();
		SendPacket_UniCast(sessionIDArr[i], cPacket, false);
	}

	// 자신 포함해서 다 보냈으니 1을 줄여야 짝이 맞는다.
	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;
	return true;
}

bool CLanServer::SetWSARecv(st_NetSession* ptr)
{
	// WSARecv
	int recvRet, recvCount = 0;
	DWORD flags = 0, recvbytes = 0;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	WSABUF recvWsa[MAX_PACKET_BATCH];
	ZeroMemory(&(ptr->recvOverlapped), sizeof(OVERLAPPED));

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

bool CLanServer::SetWSASend(st_NetSession* ptr)
{
	int retval, sendCount = 0;
	DWORD sendbytes;

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	WSABUF sendWsa[MAX_PACKET_BATCH];

	if (ptr->bCanceled)
		return false;

	RefCountPointer cpacket;
	int loopCnt = ptr->sendBuf->Size();
	if (loopCnt >= MAX_PACKET_BATCH)
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
			// 전송이 실패했으니, 여기서 다시 제거
			int cnt = ptr->dwSendCount;
			for (int i = 0; i < cnt; i++)
			{
				if (!ptr->cPacketArr[i].DecRefCount())
					_pLog._dwPacketPoolUse--;
			}
			ptr->dwSendCount = 0;
			return false;
		}
	}

	return true;
}

void CLanServer::ReleaseSession(ULONGLONG ulSessionID)
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
	while (ptr->sendBuf->Size() > 0)
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

void CLanServer::QuitServer()
{

}