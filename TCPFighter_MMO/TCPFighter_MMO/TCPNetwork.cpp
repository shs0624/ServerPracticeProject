#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <ws2tcpip.h>
#include "TCPDefine.h"
#include "PacketDefine.h"
#include "TCPNetwork.h"
#include "MessageProc.h"
#include "MessageCreate.h"
#include "Debug.h"
#include "LogProc.h"
#include <list>
#include "CStack.h"
using namespace std;

#include "SectorDefine.h"
#include "ContentsDefine.h"
#include "SectorProc.h"
#include "ContentsProc.h"
#include "ProcademyProfiler.h"
#include "CFreeList.h"
#include <unordered_map>

SOCKET	m_ListenSocket;
DWORD m_IDCnt = 1;

CStack<st_SESSION*> _disconnectStack;
unordered_map<SOCKET, st_SESSION*> _sessionMap;
procademy::CMemoryPool<st_SESSION> _sessionPool(dfMAX_CONNECT, false, false);

extern int g_iLogLevel;
extern WCHAR g_szLogBuff[1024];

extern int _selectIOFrame;

void netProc_Accept();
void netProc_Recv(SOCKET socket);
void netProc_Send(SOCKET socket);
void SelectProc(fd_set* readSet, fd_set* writeSet);

int GetSessionCount()
{
	return _sessionMap.size();
}

// 세션 풀 초기화
void InitSessionPool()
{
	st_SESSION* arr[dfMAX_CONNECT];

	for (int i = 0; i < dfMAX_CONNECT; i++)
	{
		st_SESSION* ptr = _sessionPool.Alloc();

		ptr->RecvQ = new CRingBuffer(PROTOCOL_MAXSIZE * 500);
		ptr->SendQ = new CRingBuffer(PROTOCOL_MAXSIZE * 1000);

		arr[i] = ptr;
	}

	for (int i = 0; i < dfMAX_CONNECT; i++)
	{
		_sessionPool.Free(arr[i]);
	}
}

void netStartup()
{
	srand(time(NULL));

	InitSessionPool();

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		err_quit("Startup()");

	m_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_ListenSocket == INVALID_SOCKET)
		err_quit("Listen Socket Error()");

	LINGER lingerOpt = { 0,1 };
	int lingerRet = setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(LINGER));
	if (lingerRet == SOCKET_ERROR)
		err_quit("Linger()");

	u_long on = 1;
	int ioctlRet = ioctlsocket(m_ListenSocket, FIONBIO, &on);
	if (ioctlRet == SOCKET_ERROR) 
		err_quit("ioctlsocket()");

	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(dfNETWORK_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	int bindRet = bind(m_ListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (bindRet == SOCKET_ERROR)
		err_quit("bind()");

	int listenRet = listen(m_ListenSocket, SOMAXCONN_HINT(SOMAXCONN));
	if (listenRet == SOCKET_ERROR)
		err_quit("listen()");

	_LOG(2, L"Server Open OK # Port : %d\n", dfNETWORK_PORT);
}

void netSelectIO()
{
	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;

	_selectIOFrame++;

	int loopCount = 0;
	fd_set readSet, writeSet;
	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	FD_SET(m_ListenSocket, &readSet);

	unordered_map<SOCKET, st_SESSION*>::iterator it;

	int cnt = 0;
	{
		//Profiler("Select_netSelectIO");
		it = _sessionMap.begin();
		int setSize = 1;
		while (it != _sessionMap.end())
		{
			if (setSize >= 64)
			{
				SelectProc(&readSet, &writeSet);
				FD_ZERO(&readSet);
				FD_ZERO(&writeSet);

				FD_SET(m_ListenSocket, &readSet);
				setSize = 1;
			}

			FD_SET(it->first, &readSet);
			if ((*it).second->SendQ->GetUseSize() > 0)
				FD_SET(it->first, &writeSet);

			setSize++;
			it++;
		}

		if (setSize > 0)
			SelectProc(&readSet, &writeSet);
	}

	DisconnectDeletedSession();
}

void SelectProc(fd_set* readSet, fd_set* writeSet)
{
	//Profiler("SelectProc");
	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;

	//PRO_BEGIN("select CallTime");
	int iResult = select(0, readSet, writeSet, NULL, &time);
	if (iResult == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			return;

		err_quit("select()");
	}
	//PRO_END("select CallTime");

	if (iResult > 0)
	{
		for (int i = 0; i < readSet->fd_count; i++)
		{
			//Profiler("netProc_Recv");
			if (readSet->fd_array[i] == m_ListenSocket)
			{
				netProc_Accept();
			}
			else
			{
				netProc_Recv(readSet->fd_array[i]);
			}
		}

		for (int i = 0; i < writeSet->fd_count; i++)
		{
			//Profiler("netProc_Send");
			netProc_Send(writeSet->fd_array[i]);
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
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			return;

		err_display("accept()");
		return;
	}
	else
	{
		//PRO_BEGIN("Accept");
		
		st_SESSION* session = _sessionPool.Alloc();
		if (session == NULL)
		{
			err_quit("accept_malloc");
			return;
		}

		session->dwSessionID = m_IDCnt++;
		session->RecvQ->ClearBuffer();
		session->SendQ->ClearBuffer();
		session->Socket = clientSocket;
		session->bDeleted = false;
		//session->IPPtr = clientAddr;
		session->dwLastRecvTime = timeGetTime();

		_sessionMap.insert({ session->Socket, session });

		// 플레이어 정보 생성
		if (!netPacketProc_Accept(session))
		{
			//PRO_END("Accept");
			_LOG(0, L"Player Creation Fail! # sessionID : %d # Port : %d\n", session->dwSessionID, session->IPPtr.sin_port);
			return;
		}

		//PRO_END("Accept");
		_LOG(0, L"Accepted Player # Port : %d\n", session->IPPtr.sin_port);
	}
}

void netProc_Recv(SOCKET socket)
{
	static CPacket csPacket(PROTOCOL_MAXSIZE);
	unordered_map<SOCKET, st_SESSION*>::iterator it;
	it = _sessionMap.find(socket);
	if (it == _sessionMap.end())
		return;

	st_SESSION* pSession = it->second;
	if (pSession->bDeleted)
		return;

	CRingBuffer* recvBuffer = pSession->RecvQ;
	int freeSize = recvBuffer->GetFreeSize();
	int recvSize = (recvBuffer->DirectEnqueueSize() > PROTOCOL_MAXSIZE)
		? PROTOCOL_MAXSIZE : recvBuffer->DirectEnqueueSize();

	int recvRet = recv(pSession->Socket, recvBuffer->GetRearBufferPtr(), recvSize, 0);
	if (recvRet == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			return;

		_LOG(1, L"Recv SOCKET ERROR # ERRORNUM : %d\n", WSAGetLastError());
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
	st_PACKET_HEADER header;
	csPacket.Clear();
	while (1)
	{
		if (recvBuffer->GetUseSize() < sizeof(st_PACKET_HEADER))
			break;

		int peekRet = recvBuffer->Peek((char*)&header, sizeof(st_PACKET_HEADER));
		if (peekRet != sizeof(st_PACKET_HEADER))
		{
			DebugBreak();
			DisconnectSession(pSession);
			return;
		}

		if (recvBuffer->GetUseSize() < peekRet + header.bySize)
			break;

		int dequeueRet = recvBuffer->Dequeue(csPacket.GetBufferPtr(), peekRet + header.bySize);
		csPacket.MoveWritePos(peekRet + header.bySize);
		if (dequeueRet != peekRet + header.bySize)
		{
			DebugBreak();
			DisconnectSession(pSession);
			return;
		}

		csPacket.MoveReadPos(sizeof(st_PACKET_HEADER));
		ProcessMessage(pSession, header.byType, &csPacket);
		csPacket.Clear();

		//_LOG(0, L"Received Message # Type : %d # sessionID : %d\n", header.byType, session->dwSessionID);
	}

	pSession->dwLastRecvTime = timeGetTime();
}

// 프레임 마지막에 호출해서 전송하는 함수
void netProc_Send(SOCKET socket)
{
	unordered_map<SOCKET, st_SESSION*>::iterator it;
	it = _sessionMap.find(socket);
	if (it == _sessionMap.end())
		return;

	st_SESSION* pSession = it->second;
	if (pSession->bDeleted)
		return;

	int sendRet = send(pSession->Socket, pSession->SendQ->GetFrontBufferPtr(), pSession->SendQ->DirectDequeueSize(), 0);
	if (sendRet == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
		{
			return;
		}

		_LOG(1, L"Send SOCKET ERROR # ERRORNUM : %d\n", WSAGetLastError());
		DisconnectSession(pSession);
	}
	else if (sendRet == 0)
	{
		DisconnectSession(pSession);
	}

	pSession->SendQ->MoveFront(sendRet);
}

void DisconnectSession(st_SESSION* pSession)
{
	if (pSession->bDeleted)
		return;

	pSession->bDeleted = true;
	_disconnectStack.push(pSession);
}

void DisconnectDeletedSession()
{
	while (!_disconnectStack.empty())
	{
		st_SESSION* ptr = _disconnectStack.top();
		_disconnectStack.pop();

		_LOG(0, L"Disconnect Session L4 # sessionID : %d\n", ptr->dwSessionID);

		SetDeleteCharacter(ptr->dwSessionID);

		_sessionMap.erase(ptr->Socket);

		closesocket(ptr->Socket);

		_sessionPool.Free(ptr);
		continue;
	}
}

bool Send_UniCast(st_SESSION* pSession, CPacket* cPacket)
{
	if (pSession->SendQ->GetFreeSize() < cPacket->GetDataSize())
	{
		// 연결끊기?
		DebugBreak();
		DisconnectSession(pSession);
		return false;
	}

	int ret = pSession->SendQ->Enqueue(cPacket->GetBufferPtr(), cPacket->GetDataSize());
	if (ret != cPacket->GetDataSize())
	{
		// 연결 끊기
		DebugBreak();
		DisconnectSession(pSession);
		return false;
	}

	//_LOG(0, L"Enqueue Message  # Size : %d # sessionID : %d\n", ret, pSession->dwSessionID);
	return true;
}