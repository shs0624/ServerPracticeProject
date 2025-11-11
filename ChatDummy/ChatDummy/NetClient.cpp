#pragma comment(lib,"ws2_32")
#include "Includes.h"
#include "NetClient.h"
#define IOCP_THREADCOUNT 5
#define LOGCOUNT 10000

DWORD _threadID = 0;
DWORD _logID = 0;

// 처음 1회 초기화 함수
bool CNetClient::StartNetClient()
{
	_mySession = new st_Session;

	ZeroMemory(&(_mySession->recvOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(&(_mySession->sendOverlapped), sizeof(OVERLAPPED));
	_mySession->dwSendCount = 0;
	_mySession->bSendFlag = false;
	_mySession->sendBuf = new queue<RefCountPointer>();
	_mySession->cPacketQ = new queue<RefCountPointer>();
	_mySession->recvBuf = new CRingBuffer(20000);

	int retval;

	// 굳이 쓸 이유 없을듯?
	//LINGER lingerOpt = { 0,1 };
	//int lingerRet = setsockopt(_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(LINGER));
	//if (lingerRet == SOCKET_ERROR)
	//	err_quit("Linger()");
	return true;
}

bool CNetClient::Init(SOCKADDR_IN serverAddr)
{
	_serverAddr = serverAddr;

	return true;
}

// @@TODO : 이거에서 false는 연결 끊김 뿐
// 얘는 빼서 받은 메세지를 상속받은 쪽에 OnRecv로 전달만하자.
bool CNetClient::RecvProc_Net(DWORD cbTransferred, DWORD& recvTPS)
{
	st_NetHeader netHeader;
	_mySession->recvBuf->MoveRear(cbTransferred);

	while (1)
	{
		RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
		(*csPacket)->Initialize(PROTOCOL_MAX_SIZE, 0);

		short len;
		unsigned char RK;

		// csPacket 초기화 후 ptr->recvBuf에서 Dequeue
		{
			(*csPacket)->Clear();

			int useSize = _mySession->recvBuf->GetUseSize();
			if (useSize < sizeof(st_NetHeader))
			{
				break;
			}

			int peekRet = _mySession->recvBuf->Peek((char*)(*csPacket)->GetHeadPtr(), sizeof(st_NetHeader));
			if (peekRet != sizeof(st_NetHeader))
			{
				// 클라 입장에선 이건 그냥 오작동;
				DebugBreak();
				return false;
			}
			(*csPacket)->MoveReadPos(sizeof(st_NetHeader));

			len = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->shLen;
			RK = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->RandKey;
			if (useSize < sizeof(st_NetHeader) + len)
			{
				// 클라 입장에선 이건 그냥 오작동;
				DebugBreak();
				return false;
			}

			_mySession->recvBuf->MoveFront(sizeof(st_NetHeader));
			_mySession->recvBuf->Dequeue((*csPacket)->GetTailPtr(), len);
			(*csPacket)->MoveWritePos(len);
		}

		// 디코딩, 체크섬 검사
		if (!(*csPacket)->Decode(FIXED_KEY, RK))
		{
			return false;
		}

		// netHeader만큼 이동시키고, OnRecv
		(*csPacket)->MoveReadPos(sizeof(st_NetHeader));
		if (!OnRecv(csPacket))
			return false;

		InterlockedIncrement((DWORD*)&recvTPS);
	}

	return true;
}

bool CNetClient::Connect(SOCKADDR_IN serverAddr)
{
	int retval;

	// connect전에 초기화
	ZeroMemory(&(_mySession->recvOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(&(_mySession->sendOverlapped), sizeof(OVERLAPPED));
	_mySession->dwSendCount = 0;
	_mySession->bSendFlag = false;
	_mySession->recvBuf->ClearBuffer();
	while(!_mySession->sendBuf->empty())
		_mySession->sendBuf->pop();
	while (!_mySession->cPacketQ->empty())
		_mySession->sendBuf->pop();

	// socket();
	_mySession->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (_mySession->sock == INVALID_SOCKET)
	{
		DebugBreak();
		err_quit("SOCKET()");
		return false;
	}

	int optval = 0;
	retval = setsockopt(_mySession->sock, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR)
	{
		err_quit("SO_SNDBUF()");
		return false;
	}

	// 실패가 아니라, connect 실패 띄워야 함
	int connectRet = connect(_mySession->sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (connectRet == SOCKET_ERROR)
		return false;

	SetWSARecv();

	return true;
}

// 그냥 closesocket?
bool CNetClient::Disconnect()
{
	closesocket(_mySession->sock);

	return true;
}

// 현재 SendQ를 확인하고 보낼게 있다면 보내기
bool CNetClient::SendPacket_Re(DWORD& sendTPS)
{
	int cnt = _mySession->dwSendCount;
	for (int i = 0; i < cnt; i++)
		_mySession->cPacketQ->pop();

	int size = _mySession->sendBuf->size();
	if (size > 0)
	{
		if (!SetWSASend())
		{
			DebugBreak();
			InterlockedExchange((LONG*)&(_mySession->bSendFlag), FALSE);
			return false;
		}

		InterlockedAdd((LONG*)&sendTPS, size);
	}
	
	return true;
}

// 완성된 패킷이 들어온다는 가정
bool CNetClient::SendPacket_UniCast(RefCountPointer& cPacket)
{
	st_Session* ptr = _mySession;

	// 컨텐츠가 만들어서 줄거니까, 여기서 하지 말자.
	//short shSize = (*cPacket)->GetDataSize();
	//st_NetHeader netHeader;
	//netHeader.FixedKey = FIXED_KEY;
	//netHeader.RandKey = (unsigned char)rand();
	//netHeader.shLen = shSize;

	//(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
	//(*cPacket)->Encode(FIXED_KEY);

	ptr->sendBuf->push(cPacket);

	if (InterlockedExchange((LONG*)&(ptr->bSendFlag), TRUE) != TRUE)
	{
		if (!SetWSASend())
		{
			InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
			return false;
		}
	}

	return true;
}

// 받은 iocp에 Post. IOCP로 송수신 처리 전부 떠넘기기 용도
bool CNetClient::SendPost(LPVOID ptr, RefCountPointer& cPacket, HANDLE iocpHandle)
{
	_mySession->sendBuf->push(cPacket);

	PostQueuedCompletionStatus(iocpHandle, (*cPacket)->GetDataSize(), (ULONG_PTR)ptr, &_mySession->sendOverlapped);

	return true;
}

bool CNetClient::SetWSARecv()
{
	// WSARecv
	int recvRet, recvCount = 0;
	DWORD flags = 0, recvbytes = 0;
	WSABUF recvWsa[200];
	st_Session* ptr = _mySession;

	ZeroMemory(&(_mySession->recvOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(&(_mySession->sendOverlapped), sizeof(OVERLAPPED));

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

bool CNetClient::SetWSASend()
{
	int retval, sendCount = 0;
	DWORD sendbytes;
	WSABUF sendWsa[200];

	RefCountPointer cpacket;
	int loopCnt = _mySession->sendBuf->size();
	for (int i = 0; i < loopCnt; i++)
	{
		cpacket = _mySession->sendBuf->front();
		_mySession->sendBuf->pop();
		_mySession->cPacketQ->push(cpacket);

		sendWsa[i].buf = (*cpacket)->GetBufferPtr();
		sendWsa[i].len = (*cpacket)->GetDataSize();
		sendCount++;
	}

	// @@TODO : 멀티스레드 상황을 걱정한거라, 지금은 비활성화해도 괜찮을듯.
	/*if (sendCount == 0)
	{
		return false;
	}*/

	_mySession->dwSendCount = sendCount;
	retval = WSASend(_mySession->sock, sendWsa, sendCount, &sendbytes,
		0, &(_mySession->sendOverlapped), NULL);

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