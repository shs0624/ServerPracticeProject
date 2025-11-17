#include "Includes.h"
#include "PacketDefine.h"
#include "CommonProtocol.h"
#include "LogController.h"
#include "DummyHandler.h"
#include "TCPNetwork.h"
#include "ChatDummy.h"
#include "ChatDummyController.h"


void TCPNetworkController::netStartUp(int sessionCount, int startIdx, bool bTestTimeout)
{
	srand(time(NULL));

	_iSessionCount = sessionCount;
	int idx;
	for (idx = 0; idx < _iSessionCount; idx++)
	{
		st_NetSession* ptr = new st_NetSession;

		ptr->sendBuf = new CRingBuffer(MAX_PROTOCOLSIZE * 100);
		ptr->recvBuf = new CRingBuffer(MAX_PROTOCOLSIZE * 1000);

		_sessionArr[idx] = ptr;
		_sessionArr[idx]->sock = INVALID_SOCKET;
		_sessionArr[idx]->sessionID = idx;
		_sessionArr[idx]->bConnected = false;
		_sessionArr[idx]->bConnectPending = false;
		_sessionArr[idx]->bDeleted = false;
	}

	// 타임아웃 세션 추가
}

void TCPNetworkController::netSelectIO()
{
	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;

	_iselectIOFrame++;

	int loopCount = 0;
	fd_set readSet, writeSet, exceptSet; 
	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	FD_ZERO(&exceptSet);

	int setSize = 0;
	for (int i = 0; i < _iSessionCount; i++)
	{
		if (setSize >= 64)
		{
			SelectProc(&readSet, &writeSet, &exceptSet);
			FD_ZERO(&readSet);
			FD_ZERO(&writeSet);
			setSize = 1;
		}

		if (_sessionArr[i]->sock == INVALID_SOCKET)
			continue;

		FD_SET(_sessionArr[i]->sock, &readSet);
		if (_sessionArr[i]->bConnectPending || _sessionArr[i]->sendBuf->GetUseSize() > 0)
			FD_SET(_sessionArr[i]->sock, &writeSet);
		if (_sessionArr[i]->bConnectPending)        
			FD_SET(_sessionArr[i]->sock, &exceptSet);

		setSize++;
	}

	if (setSize > 0)
		SelectProc(&readSet, &writeSet, &exceptSet);
}

void TCPNetworkController::SelectProc(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet)
{
	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;

	int iResult = select(0, readSet, writeSet, exceptSet, &time);
	if (iResult == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			return;

		//err_quit("select()");
	}

	if (iResult > 0)
	{
		for (int i = 0; i < readSet->fd_count; i++)
		{
			netProc_Recv(readSet->fd_array[i]);
		}

		for (int i = 0; i < writeSet->fd_count; i++)
		{
			netProc_Send(writeSet->fd_array[i]);
		}

		for (int i = 0; i < exceptSet->fd_count; i++)
		{
			netProc_Except(exceptSet->fd_array[i]);
		}
	}
}

bool TCPNetworkController::Connect(DWORD idx)
{
	st_NetSession* ptr = _sessionArr[idx];
	int retval;

	if (ptr->bConnectPending)
		return FALSE;

	// connect전에 초기화
	ptr->sock = INVALID_SOCKET;
	ptr->bDeleted = FALSE;
	ptr->bConnectPending = FALSE;
	ptr->bConnected = FALSE;
	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf->ClearBuffer();

	// socket();
	ptr->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ptr->sock == INVALID_SOCKET)
	{
		DebugBreak();
		err_quit("SOCKET()");
		return false;
	}

	_sessionMap.insert({ ptr->sock, ptr });

	// 논블락킹
	u_long on = 1;
	int ioctlRet = ioctlsocket(ptr->sock, FIONBIO, &on);
	if (ioctlRet == SOCKET_ERROR)
		err_quit("ioctlsocket()");

	// 실패가 아니라, connect 실패 띄워야 함
	InterlockedIncrement(&LogController::_LogController._dwConnectTry);

	int connectRet = connect(ptr->sock, (SOCKADDR*)&_serverAddr, sizeof(_serverAddr));
	if (connectRet == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK)
		{
			ptr->bConnectPending = FALSE;
			ptr->bConnected = FALSE;
			_sessionMap.erase(ptr->sock);
			InterlockedIncrement(&LogController::_LogController._dwConnectFail);
			return false;
		}

		ptr->bConnectPending = TRUE;
	}
	else
	{
		ptr->bConnected = TRUE;
		ptr->bConnectPending = FALSE;
		InterlockedIncrement(&LogController::_LogController._dwConnectSuccess);
		_dummyHandler->OnConnected(idx);
	}

	return true;
}

void TCPNetworkController::netProc_Recv(SOCKET socket)
{
	// 소켓으로 세션을 찾아야하니까 map을 써야함
	unordered_map<SOCKET, st_NetSession*>::iterator it;
	it = _sessionMap.find(socket);
	if (it == _sessionMap.end())
		return;

	st_NetSession* pSession = it->second;
	if (pSession->bDeleted)
		return;

	CRingBuffer* recvBuffer = pSession->recvBuf;
	int freeSize = recvBuffer->GetFreeSize();
	/*int recvSize = (recvBuffer->DirectEnqueueSize() > MAX_PROTOCOLSIZE)
		? MAX_PROTOCOLSIZE : recvBuffer->DirectEnqueueSize();*/
	int recvSize = recvBuffer->DirectEnqueueSize();

	int recvRet = recv(pSession->sock, recvBuffer->GetRearBufferPtr(), recvSize, 0);
	if (recvRet == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			return;

		//_LOG(1, L"Recv SOCKET ERROR # ERRORNUM : %d\n", WSAGetLastError());
		DisconnectSession(pSession);
		return;
	}

	if (recvRet == 0)
	{
		DisconnectSession(pSession);
		return;
	}

	recvBuffer->MoveRear(recvRet);

	// 헤더 읽고 처리
	st_NetHeader header;
	while (1)
	{
		RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
		(*csPacket)->Initialize(MAX_PROTOCOLSIZE, 0);

		if (recvBuffer->GetUseSize() < sizeof(st_NetHeader))
			break;

		int peekRet = recvBuffer->Peek((char*)&header, sizeof(st_NetHeader));
		if (peekRet != sizeof(st_NetHeader))
		{
			DebugBreak();
			DisconnectSession(pSession);
			return;
		}

		if (recvBuffer->GetUseSize() < peekRet + header.shLen)
			break;

		int dequeueRet = recvBuffer->Dequeue((*csPacket)->GetBufferPtr(), peekRet + header.shLen);
		(*csPacket)->MoveWritePos(peekRet + header.shLen);
		if (dequeueRet != peekRet + header.shLen)
		{
			DebugBreak();
			DisconnectSession(pSession);
			return;
		}

		(*csPacket)->MoveReadPos(sizeof(st_NetHeader));

		//@@TODO : 인코딩 잠시 비활성화
		//if (!(*csPacket)->Decode(FIXED_KEY, header.RandKey))
		//{
		//	InterlockedIncrement(&LogController::_LogController._dwResponseFailCount);
		//	(*csPacket)->Clear();
		//	continue;
		//}

		_dummyHandler->OnRecv(pSession->sessionID, csPacket);
		(*csPacket)->Clear();

		//_LOG(0, L"Received Message # Type : %d # sessionID : %d\n", header.byType, session->dwSessionID);
		InterlockedIncrement(&LogController::_LogController._dwRecvMessageTPS);
	}

	pSession->dwLastMessageTime = timeGetTime();
}

// 프레임 마지막에 호출해서 전송하는 함수
void TCPNetworkController::netProc_Send(SOCKET socket)
{
	unordered_map<SOCKET, st_NetSession*>::iterator it;
	it = _sessionMap.find(socket);
	if (it == _sessionMap.end())
		return;

	st_NetSession* pSession = it->second;
	if (pSession->bDeleted)
		return;

	if (pSession->bConnectPending)
	{
		ConnectProc(pSession);
	}
	else
	{
		SendProc(pSession);
	}
}

void TCPNetworkController::netProc_Except(SOCKET socket)
{
	auto it = _sessionMap.find(socket);
	if (it == _sessionMap.end()) {
		return;
	}

	st_NetSession* session = it->second;
	if (!session->bConnectPending) 
		return;

	int err = 0;
	int len = sizeof(err);
	getsockopt(socket, SOL_SOCKET, SO_ERROR, (char*)&err, &len);

	if (err != 0) {
		InterlockedIncrement(&LogController::_LogController._dwConnectFail);
		session->bConnectPending = false;
		_sessionMap.erase(socket);
		closesocket(socket);
		session->sock = INVALID_SOCKET;

		// 실패한 세션은 재연결 시도
		Connect(session->sessionID);
	}
}

// @@TODO : Connect 성공만 여길 타기 때문에 비활성화해둠. 확인 필요
void TCPNetworkController::ConnectProc(st_NetSession* ptr)
{
	InterlockedIncrement(&LogController::_LogController._dwConnectSuccess);

	ptr->bConnectPending = false;
	ptr->bConnected = true;
	_dummyHandler->OnConnected(ptr->sessionID);

	//int err = 0;
	//int len = sizeof(err);
	//getsockopt(ptr->sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
	//if (err == 0)
	//{
	//	InterlockedIncrement(&LogController::_LogController._dwConnectSuccess);

	//	ptr->bConnectPending = false;
	//	ptr->bConnected = true;
	//	_dummyHandler->OnConnected(ptr->sessionID);
	//}
	//else
	//{
	//	InterlockedIncrement(&LogController::_LogController._dwConnectFail);

	//	ptr->bConnectPending = false;
	//	// 다시 커넥트 시도
	//	Connect(ptr->sessionID);
	//}
}

void TCPNetworkController::SendProc(st_NetSession* ptr)
{
	int sendRet = send(ptr->sock, ptr->sendBuf->GetFrontBufferPtr(), ptr->sendBuf->GetUseSize(), 0);
	if (sendRet == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
		{
			return;
		}

		//_LOG(1, L"Send SOCKET ERROR # ERRORNUM : %d\n", WSAGetLastError());
		DisconnectSession(ptr);
		return;
	}
	else if (sendRet == 0)
	{
		DebugBreak();
		DisconnectSession(ptr);
		return;
	}

	ptr->sendBuf->MoveFront(sendRet);
	InterlockedIncrement(&LogController::_LogController._dwSendMessageTPS);
}

// 컨텐츠에서 헤더 없이 완성된 패킷을 보낸다.
void TCPNetworkController::SendPacket(DWORD sessionID, RefCountPointer& cPacket)
{
	// 40 + 40 + 64 - id, nickname, sessionKey
	st_NetHeader header;
	header.FixedKey = FIXED_KEY;
	header.RandKey = rand() % 256;
	header.shLen = (*cPacket)->GetDataSize();
	
	//@@TODO : 인코딩 잠시 비활성화
	// 여기서 체크섬까지 다 넣고 인코딩해줌
	//(*cPacket)->Encode(FIXED_KEY);
	(*cPacket)->PushHeader((char*)&header, sizeof(st_NetHeader));
	(*cPacket)->SetCheckSum();

	st_NetSession* pSession = _sessionArr[sessionID];
	if (pSession->sendBuf->GetFreeSize() < (*cPacket)->GetDataSize())
	{
		// 연결끊기?
		DebugBreak();
		DisconnectSession(pSession);
		return;
	}

	int ret = pSession->sendBuf->Enqueue((*cPacket)->GetBufferPtr(), (*cPacket)->GetDataSize());
	if (ret != (*cPacket)->GetDataSize())
	{
		// 연결 끊기
		DebugBreak();
		DisconnectSession(pSession);
		return;
	}

	//_LOG(0, L"Enqueue Message  # Size : %d # sessionID : %d\n", ret, pSession->dwSessionID);
	// @@TODO : sendTPS 올리기
}

void TCPNetworkController::DisconnectSession(DWORD sessionID)
{
	st_NetSession* pSession = _sessionArr[sessionID];

	if (pSession->bDeleted)
		return;

	_sessionMap.erase(pSession->sock);
	closesocket(pSession->sock);

	pSession->sock = INVALID_SOCKET;
	pSession->bConnectPending = false;
	pSession->bDeleted = true;
	pSession->bConnected = false;
	_dummyHandler->OnDisconnect(pSession->sessionID);
}

void TCPNetworkController::DisconnectSession(st_NetSession* pSession)
{
	if (pSession->bDeleted)
		return;

	_sessionMap.erase(pSession->sock);
	closesocket(pSession->sock);

	pSession->sock = INVALID_SOCKET;
	pSession->bConnectPending = false;
	pSession->bDeleted = true;
	pSession->bConnected = false;
	_dummyHandler->OnDisconnect(pSession->sessionID);
}