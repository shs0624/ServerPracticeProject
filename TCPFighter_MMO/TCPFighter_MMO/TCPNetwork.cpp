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

DWORD dwNetworkCurrentTick;
SOCKET	m_ListenSocket;
DWORD m_IDCnt = 0;

CStack <st_SESSION*> _disconnectStack;
unordered_map<DWORD, st_SESSION*> _sessionMap;
procademy::CMemoryPool<st_SESSION> _sessionPool(dfMAX_CONNECT, false, false);

extern int g_iLogLevel;
extern WCHAR g_szLogBuff[1024];

extern int _selectIOFrame;

void netProc_Accept();
void netProc_Recv(st_SESSION* session);
void netProc_Send(st_SESSION* session);
void SelectProc(fd_set* readSet, fd_set* writeSet, CStack<st_SESSION*>& selectStack);

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

		ptr->RecvQ = new CRingBuffer(PROTOCOL_MAXSIZE * 300);
		ptr->SendQ = new CRingBuffer(PROTOCOL_MAXSIZE * 600);

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
	int lingerRet = setsockopt(m_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(lingerOpt));
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
	static CStack<st_SESSION*> selectStack(FD_SETSIZE + 1);
	selectStack.clear();

	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;

	_selectIOFrame++;

	int loopCount = 0;
	fd_set readSet, writeSet;
	FD_ZERO(&readSet);
	FD_ZERO(&writeSet);
	FD_SET(m_ListenSocket, &readSet);

	unordered_map<DWORD, st_SESSION*>::iterator it;

	int iResult = select(0, &readSet, NULL, NULL, &time);
	if (iResult == SOCKET_ERROR)
		err_quit("select()");

	// accept
	if (FD_ISSET(m_ListenSocket, &readSet))
	{
		netProc_Accept();
	}
	
	DWORD oldTick = dwNetworkCurrentTick;
	dwNetworkCurrentTick = timeGetTime();

	if (_sessionMap.empty())
		return;

	{
		//Profiler("Select_netSelectIO");
		it = _sessionMap.begin();
		int setSize = 0;
		while (it != _sessionMap.end())
		{
			if (setSize >= 64)
			{
				SelectProc(&readSet, &writeSet, selectStack);

				FD_ZERO(&readSet);
				FD_ZERO(&writeSet);
				selectStack.clear();
				setSize = 0;
			}

			if ((*it).second->bDeleted)
			{
				it++;
				continue;
			}

			// 타임아웃 체크는 L4에서
			if (dwNetworkCurrentTick - (it)->second->dwLastRecvTime > dfNETWORK_PACKET_RECV_TIMEOUT)
			{
				// 타임아웃
				//_LOG(2, L"TimeOut Session # ID : %d\n", it->second->dwSessionID);
				DisconnectSession(it->second->dwSessionID);
				it++;
				continue;
			}

			FD_SET((*it).second->Socket, &readSet);
			if ((*it).second->SendQ->GetUseSize() > 0)
				FD_SET((*it).second->Socket, &writeSet);

			selectStack.push(it->second);
			setSize++;
			it++;
		}

		if (setSize > 0)
			SelectProc(&readSet, &writeSet, selectStack);
	}
}

void SelectProc(fd_set* readSet, fd_set* writeSet, CStack<st_SESSION*>& selectStack)
{
	//Profiler("SelectProc");
	timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;

	//PRO_BEGIN("select CallTime");
	int iResult = select(0, readSet, writeSet, NULL, &time);
	if (iResult == SOCKET_ERROR)
	{
		if (iResult == WSAEWOULDBLOCK)
			return;

		err_quit("select()");
	}
	//PRO_END("select CallTime");


	if (iResult > 0)
	{
		while (!selectStack.empty())
		{
			st_SESSION* ptr = selectStack.top();
			selectStack.pop();

			if (ptr->bDeleted)
				continue;

			if (FD_ISSET(ptr->Socket, readSet))
			{
				//Profiler("netProcRecv");
				netProc_Recv(ptr);
			}

			if (FD_ISSET(ptr->Socket, writeSet))
			{
				//Profiler("netProcSend");
				netProc_Send(ptr);
			}
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

		_sessionMap.insert({ session->dwSessionID, session });

		// 플레이어 정보 생성
		if (!netPacketProc_Accept(session->dwSessionID))
		{
			//PRO_END("Accept");
			_LOG(0, L"Player Creation Fail! # sessionID : %d # Port : %d\n", session->dwSessionID, session->IPPtr.sin_port);
			return;
		}

		//PRO_END("Accept");
		_LOG(0, L"Accepted Player # Port : %d\n", session->IPPtr.sin_port);
	}
}

void netProc_Recv(st_SESSION* session)
{
	static CPacket csPacket(PROTOCOL_MAXSIZE);

	CRingBuffer* recvBuffer = session->RecvQ;
	char* tailPtr = recvBuffer->GetRearBufferPtr();

	int freeSize = recvBuffer->GetFreeSize();
	int recvSize = (recvBuffer->DirectEnqueueSize() > PROTOCOL_MAXSIZE) 
		? PROTOCOL_MAXSIZE : recvBuffer->DirectEnqueueSize();

	int recvRet = recv(session->Socket, tailPtr, recvSize, 0);
	if (recvRet == SOCKET_ERROR)
	{
		if (recvRet != WSAEWOULDBLOCK)
		{
			//err_display("recv()");
			DisconnectSession(session->dwSessionID);
			return;
		}
		else
		{
			return;
		}
	}

	if (recvRet == 0)
	{
		DisconnectSession(session->dwSessionID);
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
			DisconnectSession(session->dwSessionID);
			return;
		}

		if (recvBuffer->GetUseSize() < peekRet + header.bySize)
			break;

		int dequeueRet = recvBuffer->Dequeue(csPacket.GetBufferPtr(), peekRet + header.bySize);
		csPacket.MoveWritePos(peekRet + header.bySize);
		if (dequeueRet != peekRet + header.bySize)
		{
			DebugBreak();
			DisconnectSession(session->dwSessionID);
			return;
		}

		csPacket.MoveReadPos(sizeof(st_PACKET_HEADER));
		ProcessMessage(session->dwSessionID, header.byType, &csPacket);
		csPacket.Clear();

		//_LOG(0, L"Received Message # Type : %d # sessionID : %d\n", header.byType, session->dwSessionID);
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
			if (sendRet == WSAEWOULDBLOCK)
			{
				//err_display("send()_WOULDBLOCK");
				return;
			}

			//err_display("send()");
			DisconnectSession(session->dwSessionID);
			return;
		}
		else if (sendRet != sendSize)
		{
			//printf("SendRet Size _ netProc_Send sendRet : %d | sendSize : %d\n", sendRet, sendSize);
			DisconnectSession(session->dwSessionID);
			return;
		}

		sendBuffer->MoveFront(sendSize);
		//_LOG(0, L"Send Message  Size : %d # sessionID : %d\n", sendRet, session->dwSessionID);
	}
}

void DisconnectSession(DWORD dwSessionID)
{
	st_SESSION* ptr = _sessionMap.find(dwSessionID)->second;

	ptr->bDeleted = true;
	_disconnectStack.push(ptr);
}

void DisconnectDeletedSession()
{
	while (!_disconnectStack.empty())
	{
		st_SESSION* ptr = _disconnectStack.top();
		_disconnectStack.pop();

		//delete(ptr->RecvQ);
		//delete(ptr->SendQ);
		// 3. closeSocket, new-delete 과정 진행
		closesocket(ptr->Socket);

		_LOG(0, L"Disconnect Session L4 # sessionID : %d\n", ptr->dwSessionID);

		SetDeleteCharacter(ptr->dwSessionID);

		_sessionMap.erase(ptr->dwSessionID);
		_sessionPool.Free(ptr);
		continue;
	}
}

bool Send_UniCast(DWORD dwsessionID, CPacket* cPacket)
{
	std::unordered_map<DWORD, st_SESSION*>::iterator it;
	it = _sessionMap.find(dwsessionID);
	if (it == _sessionMap.end())
		return false;

	st_SESSION* pSession = (_sessionMap.find(dwsessionID))->second;

	if (pSession->SendQ->GetFreeSize() < cPacket->GetDataSize())
	{
		// 연결끊기?
		DebugBreak();
		DisconnectSession(dwsessionID);
		return false;
	}

	int ret = pSession->SendQ->Enqueue(cPacket->GetBufferPtr(), cPacket->GetDataSize());
	if (ret != cPacket->GetDataSize())
	{
		// 연결 끊기
		DebugBreak();
		DisconnectSession(dwsessionID);
		return false;
	}

	//_LOG(0, L"Enqueue Message  # Size : %d # sessionID : %d\n", ret, pSession->dwSessionID);
	return true;
}