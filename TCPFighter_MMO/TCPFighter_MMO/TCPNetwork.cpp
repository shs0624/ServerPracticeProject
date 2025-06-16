#include "TCPDefine.h"
#include "PacketDefine.h"
#include "TCPNetwork.h"
#include "MessageProc.h"
#include "Debug.h"
#include "LogProc.h"
#include <list>
using namespace std;

#include "SectorDefine.h"
#include "ContentsDefine.h"
#include "SectorProc.h"
#include "ContentsProc.h"
#include <unordered_map>

DWORD dwNetworkCurrentTick;
SOCKET	m_ListenSocket;
DWORD m_IDCnt = 0;

unordered_map<DWORD, st_SESSION*> _sessionMap;

extern int g_iLogLevel;
extern WCHAR g_szLogBuff[1024];


void netProc_Accept();
void netProc_Recv(st_SESSION* session);
void netProc_Send(st_SESSION* session);
void SelectProc(fd_set* readSet, fd_set* writeSet, list<st_SESSION*> selectList);

void netStartup()
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		err_quit("Startup()");

	m_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_ListenSocket == INVALID_SOCKET)
		err_quit("Listen Socket Error()");

	LINGER lingerOpt = { 0,1 };
	int lingerRet = setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(lingerOpt));
	if (lingerRet == SOCKET_ERROR)
		err_quit("Linger()");

	/*u_long on = 1;
	int ioctlRet = ioctlsocket(m_ListenSocket, FIONBIO, &on);
	if (ioctlRet == SOCKET_ERROR) 
		err_quit("ioctlsocket()");*/

	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(dfNETWORK_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	int bindRet = bind(m_ListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (bindRet == SOCKET_ERROR)
		err_quit("bind()");

	int listenRet = listen(m_ListenSocket, SOMAXCONN);
	if (listenRet == SOCKET_ERROR)
		err_quit("listen()");

	_LOG(2, L"Server Open OK # Port : %d\n", dfNETWORK_PORT);
}

void netSelectIO()
{
	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;

	DWORD oldTick = dwNetworkCurrentTick;
	dwNetworkCurrentTick = timeGetTime();

	int loopCount = 0;
	fd_set readSet, writeSet;
	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	FD_SET(m_ListenSocket, &readSet);

	list<st_SESSION*> selectList;
	unordered_map<DWORD, st_SESSION*>::iterator it;

	int iResult = select(0, &readSet, NULL, NULL, &time);
	if (iResult == SOCKET_ERROR)
		err_quit("select()");

	// accept
	if (FD_ISSET(m_ListenSocket, &readSet))
	{
		netProc_Accept();
	}
	
	if (_sessionMap.empty())
		return;

	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	// recv, send 
	for (it = _sessionMap.begin(); it != _sessionMap.end(); it++)
	{
		// 타임아웃 체크는 L4에서
		if (dwNetworkCurrentTick - (it)->second->dwLastRecvTime > dfNETWORK_PACKET_RECV_TIMEOUT)
		{
			// 타임아웃
			DebugBreak();
			DisconnectSession(it->second->dwSessionID);
			continue;
		}

		FD_SET((*it).second->Socket, &readSet);
		if ((*it).second->SendQ->GetUseSize() > 0)
			FD_SET((*it).second->Socket, &writeSet);

		selectList.push_back((*it).second);

		loopCount++;
		if (loopCount >= FD_SETSIZE)
		{
			SelectProc(&readSet, &writeSet, selectList);

			FD_ZERO(&readSet);
			FD_ZERO(&writeSet);
			selectList.clear();
			loopCount = 0;
		}
	}

	// 남은 readSet, writeSet 처리
	SelectProc(&readSet, &writeSet, selectList);
}

void SelectProc(fd_set* readSet, fd_set* writeSet, list<st_SESSION*> selectList)
{
	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;

	int iResult = select(0, readSet, writeSet, NULL, &time);
	if (iResult == SOCKET_ERROR)
		err_quit("select()");

	list<st_SESSION*>::iterator it;
	for (it = selectList.begin(); it != selectList.end(); it++)
	{
		if (FD_ISSET((*it)->Socket, readSet))
		{
			netProc_Recv(*it);
		}
	}

	// send
	for (it = selectList.begin(); it != selectList.end(); it++)
	{
		if (FD_ISSET((*it)->Socket, writeSet))
		{
			netProc_Send(*it);
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
		session->IPPtr = clientAddr;
		session->dwLastRecvTime = timeGetTime();

		_sessionMap.insert({ session->dwSessionID, session });

		// 플레이어 정보 생성
		if (!netPacketProc_Accept(session->dwSessionID))
		{
			_LOG(0, L"Player Creation Fail! # sessionID : %d # Port : %d\n", session->dwSessionID, session->IPPtr.sin_port);
			return;
		}

		_LOG(0, L"Accepted Player # Port : %d\n", session->IPPtr.sin_port);
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
		SetDeleteCharacter(session->dwSessionID);
		session->bDeleted = true;
		return;
	}

	if (recvRet == 0)
	{
		SetDeleteCharacter(session->dwSessionID);
		session->bDeleted = true;
		return;
	}

	if (recvRet > freeSize)
	{
		// 공간이 없는 경우인데, 혹시 몰라 예외처리
		err_display("enqueue Fail!");
		SetDeleteCharacter(session->dwSessionID);
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
		ProcessMessage(session->dwSessionID, header.byType, csPacket);
		csPacket->Clear();

		_LOG(0, L"Received Message # Type : %d # sessionID : %d\n", header.byType, session->dwSessionID);
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
			SetDeleteCharacter(session->dwSessionID);
			session->bDeleted = true;
			return;
		}
		else if (sendRet != sendSize)
		{
			printf("SendRet Size _ netProc_Send sendRet : %d | sendSize : %d\n", sendRet, sendSize);
			SetDeleteCharacter(session->dwSessionID);
			session->bDeleted = true;
			return;
		}

		sendBuffer->MoveFront(sendSize);
		_LOG(0, L"Send Message  Size : %d # sessionID : %d\n", sendRet, session->dwSessionID);
	}

	session->dwLastRecvTime = timeGetTime(); 
}

void DisconnectSession(DWORD dwSessionID)
{
	(_sessionMap.find(dwSessionID))->second->bDeleted = true;
}

void DisconnectDeletedSession()
{
	unordered_map<DWORD, st_SESSION*>::iterator it;
	for (it = _sessionMap.begin(); it != _sessionMap.end();)
	{
		if ((*it).second->bDeleted)
		{
			delete((*it).second->RecvQ);
			delete((*it).second->SendQ);
			// 3. closeSocket, new-delete 과정 진행
			closesocket((*it).second->Socket);

			_LOG(0, L"Disconnect Session L4 # sessionID : %d\n", (*it).second->dwSessionID);
			it = _sessionMap.erase(it);
			continue;
		}

		it++;
	}
}

bool Send_UniCast(DWORD dwsessionID, st_PACKET_HEADER* header, char* packet)
{
	std::unordered_map<DWORD, st_SESSION*>::iterator it;
	it = _sessionMap.find(dwsessionID);
	if (it == _sessionMap.end())
		return false;

	st_SESSION* pSession;
	pSession = (_sessionMap.find(dwsessionID))->second;

	if (pSession->SendQ->GetFreeSize() < sizeof(st_PACKET_HEADER) + header->bySize)
	{
		// 연결끊기?
		DebugBreak();
		DisconnectSession(dwsessionID);
		return false;
	}

	int ret = pSession->SendQ->Enqueue((char*)header, sizeof(st_PACKET_HEADER));
	if (ret != sizeof(st_PACKET_HEADER))
	{
		// 연결 끊기
		DebugBreak();
		DisconnectSession(dwsessionID);
		return false;
	}

	ret = pSession->SendQ->Enqueue(packet, header->bySize);
	if (ret != header->bySize)
	{
		// 연결 끊기
		DebugBreak();
		DisconnectSession(dwsessionID);
		return false;
	}

	//_LOG(0, L"Enqueue Message  # Size : %d # type : %d # sessionID : %d\n", ret, header->byType, pSession->dwSessionID);
	return true;
}