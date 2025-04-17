#include "TCPDefine.h"
#include "PacketDefine.h"
#include "TCPNetwork.h"
#include "MessageProc.h"
#include "Debug.h"
#include <list>
using namespace std;

SOCKET	m_ListenSocket;
unsigned long m_IDCnt = 0;

list<st_SESSION*> _sessionList;

void netStartup()
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		err_quit("Startup()");

	m_ListenSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (m_ListenSocket == INVALID_SOCKET)
		err_quit("Listen Socket Error()");

	LINGER lingerOpt = { 0,1 };
	int lingerRet = setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(lingerOpt));
	if (lingerRet == SOCKET_ERROR)
		err_quit("Linger()");

	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	int bindRet = bind(m_ListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (bindRet == SOCKET_ERROR)
		err_quit("Bind()");

	int listenRet = listen(m_ListenSocket, SOMAXCONN);
	if (listenRet == SOCKET_ERROR)
		err_quit("listen()");
}

void netSelectIO()
{
	fd_set readSet, writeSet;
	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);

	FD_SET(m_ListenSocket, &readSet);

	list<st_SESSION*>::iterator it;
	for (it = _sessionList.begin(); it != _sessionList.end(); it++)
	{
		FD_SET((*it)->Socket, &readSet);
		if ((*it)->SendQ->GetUseSize() > 0)
			FD_SET((*it)->Socket, &writeSet);
	}

	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;

	int loopCount = _sessionList.size();

	// accept
	if (FD_ISSET(m_ListenSocket, &readSet))
	{
		netProc_Accept();
	}

	it = _sessionList.begin();
	while (loopCount > 0)
	{
		int selectResult = select(0, &readSet, &writeSet, NULL, &time);
		if (selectResult == SOCKET_ERROR)
			err_display("select()");

		for (int i = 0; i < selectResult; i++)
		{
			//Recv
			if (FD_ISSET((*it)->Socket, &readSet))
			{
				--loopCount;
				netProc_Recv(*it);
			}

			// send
			if (FD_ISSET((*it)->Socket, &writeSet))
			{
				--loopCount;
				netProc_Send(*it);
			}

			it++;
		}
	}
}

void netProc_Accept()
{
	int addrlen;
	SOCKET clientSocket;
	SOCKADDR_IN clientAddr;

	addrlen = sizeof(clientAddr);
	clientSocket = accept(m_ListenSocket, (SOCKADDR*)&clientAddr, &addrlen);
	if (clientSocket == INVALID_SOCKET)
	{
		if (clientSocket == WSAEWOULDBLOCK)
		{
			return;
		}

		err_display("accept()");
		return;
	}
	else
	{
		st_SESSION* session = (st_SESSION*)malloc(sizeof(st_SESSION));
		if (session == NULL)
		{
			err_quit("accept_malloc");
			return;
		}

		session->dwSessionID = m_IDCnt++;
		session->RecvQ = new CRingBuffer(PROTOCOL_MAXSIZE * 1000);
		session->SendQ = new CRingBuffer(PROTOCOL_MAXSIZE * 1000);
		session->Socket = clientSocket;
		session->bDeleted = false;
		session->dwLastRecvTime = timeGetTime();

		// 플레이어 정보 생성
		netPacketProc_Accept(session);
	}
}

void netProc_Recv(st_SESSION* session)
{
	CRingBuffer* recvBuffer = session->RecvQ;
	char* tailPtr = recvBuffer->GetRearBufferPtr();

	int freeSize = recvBuffer->GetFreeSize();
	int recvSize = (recvBuffer->DirectEnqueueSize() > PROTOCOL_MAXSIZE) 
		? PROTOCOL_MAXSIZE : recvBuffer->DirectEnqueueSize();

	int recvRet = recv(session->Socket, tailPtr, recvSize, 0);
	if (recvRet == SOCKET_ERROR)
	{
		if (recvRet == WSAEWOULDBLOCK)
			return;

		err_display("recv()");
		session->bDeleted = true;
		return;
	}

	if (recvRet > freeSize)
	{
		// 공간이 없는 경우인데, 혹시 몰라 예외처리
		err_display("enqueue Fail!");
		session->bDeleted = true;
		return;
	}

	recvBuffer->MoveRear(recvRet);

	// 헤더 읽고 처리
	st_PACKET_HEADER header;
	CPacket* csPacket = new CPacket(PROTOCOL_MAXSIZE);
	while (1)
	{
		if (recvBuffer->GetUseSize() < sizeof(st_PACKET_HEADER))
			break;

		int peekRet = recvBuffer->Peek((char*)&header, sizeof(st_PACKET_HEADER));
		if (peekRet != sizeof(st_PACKET_HEADER))
		{
			err_display("netProc_Recv peekRet != sizeof(header)");
			DebugBreak();
			return;
		}

		if (recvBuffer->GetUseSize() < peekRet + header.bySize)
			break;

		int dequeueRet = recvBuffer->Dequeue(csPacket->GetBufferPtr(), peekRet + header.bySize);
		csPacket->MoveWritePos(peekRet + header.bySize);
		if (dequeueRet != peekRet + header.bySize)
		{
			err_display("netProc_Recv dequeueRet != msgSize");
			DebugBreak();
			return;
		}

		csPacket->MoveReadPos(sizeof(st_PACKET_HEADER));
		ProcessMessage(session, header.byType, csPacket);
		csPacket->Clear();
	}

	session->dwLastRecvTime = timeGetTime();
}

// 프레임 마지막에 호출해서 전송하는 함수
void netProc_Send(st_SESSION* session)
{
	BYTE type;
	CRingBuffer* sendBuffer = session->SendQ;

	while (1)
	{
		// 보낼 수 있는 사이즈 얻기
		int useSize = sendBuffer->GetUseSize();
		int sendSize = (sendBuffer->DirectDequeueSize() > useSize) ? useSize : sendBuffer->DirectDequeueSize();

		// sendQ 확인하기
		if (useSize == 0)
		{
			break;
		}

		char* headPtr = sendBuffer->GetFrontBufferPtr();
		int sendRet = send(session->Socket, headPtr, sendSize, 0);
		if (sendRet == SOCKET_ERROR)
		{
			err_display("send()");
			session->bDeleted = true;
			return;
		}
		else if (sendRet != sendSize)
		{
			printf("SendRet Size _ netProc_Send sendRet : %d | sendSize : %d\n", sendRet, sendSize);
			session->bDeleted = true;
			return;
		}

		sendBuffer->MoveFront(sendSize);
	}

	session->dwLastRecvTime = timeGetTime(); 
}

void DisconnectSession(st_SESSION* pSession)
{
	// 1. GetAroundSector로 주변 섹터 얻어옴

	// 2. 그 섹터의 플레이어에게 DeleteCharacter 전송

	// 3. closeSocket, new-delete 과정 진행
}

void Send_UniCast(st_SESSION* Session, st_PACKET_HEADER* header, char* packet)
{

}