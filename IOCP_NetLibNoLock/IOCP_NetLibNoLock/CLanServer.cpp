#pragma comment(lib,"ws2_32")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include <unordered_map>
#include "Debug.h"
#include "CLanServer.h"
#include "ProcademyProfiler.h"
#include <crtdbg.h>
#include <iostream>
#include <minidumpapiset.h>
#include "CCrashDump.h"

procademy::CCrashDump cCrashDump;
SOCKET listen_sock;

HANDLE _tpsThreadHandle;
HANDLE _acceptThreadHandle;
HANDLE _iocpHandle;
HANDLE _iocpWorkerThreadHandleArr[50];

CRITICAL_SECTION _csProfilerCS;

unsigned int _tpsThreadID;
unsigned int _acceptThreadID;
unsigned int _iocpWorkerThreadID[50];

bool _bServerEnabled = true;

bool CLanServer::Start(ULONG ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, int maxConnection)
{
	int retval;

	InitializeSessions(maxConnection);

	// À©¼Ó ÃÊ±âÈ­
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrentThreads);
	if (_iocpHandle == NULL) return 1;

	// socket();
	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET)
		err_quit("socket()");

	int optval = 0;
	retval = setsockopt(listen_sock, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR)
		err_quit("SO_SNDBUF()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(port);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		err_quit("listen()");

	_acceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)this, 0, &_acceptThreadID);
	if (_acceptThreadHandle == NULL)
		return 1;

	_hTPSUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	_tpsThreadHandle = (HANDLE)_beginthreadex(NULL, 0, TPSThread, (LPVOID)this, 0, &_tpsThreadID);
	if (_tpsThreadHandle == NULL)
		return 1;

	_workerCount = workerCount;
	for (int i = 0; i < workerCount; i++)
	{
		_iocpWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, (LPVOID)this, 0, &_iocpWorkerThreadID[i]);
		if (_iocpWorkerThreadHandleArr[i] == NULL)
			return 1;
	}

	printf("\n[TCP ¼­¹ö] ½ÃÀÛ\n");
}

void CLanServer::InitializeSessions(int maxConnection)
{
	_iSessionCount = 0;
	_bServerEnabled = true;
	_imaxConnection = maxConnection;
	InitializeCriticalSection(&_csProfilerCS);
	_sessionArr = (st_Session*)malloc(sizeof(st_Session) * maxConnection);


	for (int i = 0; i < _imaxConnection; i++)
	{
		ZeroMemory(&_sessionArr[i].recvOverlapped, sizeof(_sessionArr[i].recvOverlapped));
		ZeroMemory(&_sessionArr[i].sendOverlapped, sizeof(_sessionArr[i].sendOverlapped));
		_sessionArr[i].bSendFlag = false;
		_sessionArr[i].bSessionUsing = false;
		_sessionArr[i].recvBuf = new CRingBuffer(15000);
		_sessionArr[i].sendBuf = new CRingBuffer(15000);
	}
}

unsigned int WINAPI CLanServer::AcceptThread(LPVOID arg)
{
	// static ¼±¾ðÇØ¼­ ÇÔ¼ö È£ÃâÀ» À§ÇÑ Æ÷ÀÎÅÍ
	CLanServer* thisPtr = (CLanServer*)arg;

	// µ¥ÀÌÅÍ Åë½Å¿¡ »ç¿ëÇÒ º¯¼ö
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	DWORD recvbytes, flags;
	char ipbuffer[50];

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
	// µ¥ÀÌÅÍ Åë½Å¿¡ »ç¿ëÇÒ º¯¼ö
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;

	addrlen = sizeof(clientaddr);
	client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
	if (client_sock == INVALID_SOCKET)
	{
		err_display("accept()");
		return false;
	}
	{
		//Profiler pro(L"AcceptProc");
		if (!thisPtr->OnConnectionRequest(clientaddr.sin_addr.S_un.S_addr, clientaddr.sin_port))
		{
			return false;
		}

		int index;
		{
			{
				Profiler("GetIndex");
				// »ç¿ë ¾ÈÇÏ´Â ¼¼¼Ç Ã£¾Æ¼­ µî·Ï
				index = 0;
				for (int i = 0; i < _imaxConnection; i++)
				{
					if (!_sessionArr[i].bSessionUsing)
					{
						index = i;
						break;
					}
				}
			}

			ZeroMemory(&_sessionArr[index].recvOverlapped, sizeof(_sessionArr[index].recvOverlapped));
			ZeroMemory(&_sessionArr[index].sendOverlapped, sizeof(_sessionArr[index].sendOverlapped));
			_sessionArr[index].ulSessionID = _threadID++;
			_sessionArr[index].dwIOCount = 0;
			_sessionArr[index].bSendFlag = false;
			_sessionArr[index].sock = client_sock;
			_sessionArr[index].recvBuf->ClearBuffer();
			_sessionArr[index].sendBuf->ClearBuffer();

			// ¼ÒÄÏÀ» IOCP¿¡ µî·Ï
			CreateIoCompletionPort((HANDLE)client_sock, _iocpHandle, (ULONG_PTR)&_sessionArr[index], 0);
			_sessionArr[index].bSessionUsing = true;
		}

		thisPtr->OnAccept();
		_iSessionCount++;
		InterlockedIncrement((unsigned int*)&_iAcceptTPS);

		SetWSARecv(&_sessionArr[index]);
	}
	return true;
}

unsigned int WINAPI CLanServer::IOCPWorkerThread(LPVOID arg)
{
	char ipbuffer[50];
	char tempBuffer[PROTOCOL_MAX_SIZE + 1];
	int retval;
	CLanServer* thisPtr = (CLanServer*)arg;

	while (1)
	{
		WSABUF wsabuf;
		DWORD cbTransferred = 0, recvbytes;
		SOCKET client_sock;
		st_Session* ptr = NULL;
		OVERLAPPED* pOverlapped;
		retval = GetQueuedCompletionStatus(_iocpHandle, &cbTransferred,
			(PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (pOverlapped == 0 && cbTransferred == 0 && ptr == 0)
		{
			// Á¾·á
			printf("IOCP Worker Thread Exit\n");
			break;
		}

		if (retval == 0 || cbTransferred == 0)
		{
			if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
			{
				// ¿¬°á ²÷±â
				thisPtr->ReleaseSession(ptr);
			}
			continue;
		}

		if (&(ptr->recvOverlapped) == pOverlapped)
		{
			{
				//Profiler pro(L"RecvOverlapped");
				if (!(thisPtr->RecvProc(ptr, cbTransferred)))
				{
					if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
					{
						// ¿¬°á ²÷±â
						thisPtr->ReleaseSession(ptr);
					}
					continue;
				}

				// WSARecv
				if (!(thisPtr->SetWSARecv(ptr)))
				{
					if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
					{
						// ¿¬°á ²÷±â
						thisPtr->ReleaseSession(ptr);
					}
					continue;
				}
			}
		}
		else
		{
			//PRO_BEGIN(L"SendOverlapped");
			ptr->sendBuf->MoveFront(cbTransferred);
			int useSize = ptr->sendBuf->GetUseSize();
			if (useSize > 0)
			{
				thisPtr->SetWSASend(ptr);
			}
			else
			{
				InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
			}
			//PRO_END(L"SendOverlapped");
		}

		if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
		{
			// ¿¬°á ²÷±â
			thisPtr->ReleaseSession(ptr);
		}
	}

	return 1;
}

void CLanServer::QuitServer()
{
	printf("CLanServer::Quit();\n");
	_bServerEnabled = false;

	for (int i = 0; i < _imaxConnection; i++)
	{
 		closesocket(_sessionArr[i].sock);
	}

	for (int i = 0; i < _workerCount; i++)
	{
		PostQueuedCompletionStatus(_iocpHandle, 0, 0, 0);
	}
}

bool CLanServer::Disconnect(ULONG sessionID)
{
	st_Session* pSession = NULL;
	EnterCriticalSection(&_csProfilerCS);
	{
		Profiler("GetSession");
		GetSession(sessionID, &pSession);
		if (pSession == NULL)
		{
			LeaveCriticalSection(&_csProfilerCS);
			return false;
		}
	}
	EnterCriticalSection(&_csProfilerCS);
	closesocket(pSession->sock);

	return true;
}

bool CLanServer::SendPacket(ULONG sessionID, CPacket* cPacket)
{
	char temp[PROTOCOL_MAX_SIZE + 1];

	st_Session* pSession = NULL;
	EnterCriticalSection(&_csProfilerCS);
	{
		Profiler("GetSession");
		GetSession(sessionID, &pSession);
		if (pSession == NULL)
		{
			LeaveCriticalSection(&_csProfilerCS);
			return false;
		}
	}
	LeaveCriticalSection(&_csProfilerCS);

	short shSize = cPacket->GetDataSize();
	cPacket->GetData(temp, shSize);

	st_NetHeader header;
	header.shLen = shSize;
	
	if (pSession->sendBuf->GetFreeSize() < sizeof(st_NetHeader) + shSize)
	{
		DebugBreak();
		Disconnect(sessionID);
		return false;
	}

	int ret = pSession->sendBuf->Enqueue((char*)&header, sizeof(st_NetHeader));
	if (ret != sizeof(st_NetHeader))
	{
		// ¿¬°á ²÷±â
		DebugBreak();
		Disconnect(sessionID);
		return false;
	}

	ret = pSession->sendBuf->Enqueue(temp, shSize);
	if (ret != header.shLen)
	{
		// ¿¬°á ²÷±â
		DebugBreak();
		Disconnect(sessionID);
		return false;
	}

	if (InterlockedExchange((LONG*)&(pSession->bSendFlag), TRUE) != TRUE)
	{
		SetWSASend(pSession);
	}

	InterlockedIncrement((unsigned int*) & _iSendMessageTPS);

	return true;
}

void CLanServer::GetSession(ULONG ulSessionID, st_Session** pSession)
{
	Profiler("FindIdx");
	for (int i = 0; i < _imaxConnection; i++)
	{
		if (_sessionArr[i].bSessionUsing && _sessionArr[i].ulSessionID == ulSessionID)
		{
			*pSession = &_sessionArr[i];
			return;
		}
	}

	*pSession = NULL;
	return;
}

unsigned int WINAPI CLanServer::TPSThread(LPVOID arg)
{
	CLanServer* thisPtr = (CLanServer*)arg;
	while (1)
	{
		thisPtr->ResetTPS();
	}
}

void CLanServer::ResetTPS()
{
	_iAcceptTPS = 0;
	_iRecvMessageTPS = 0;
	_iSendMessageTPS = 0;

	WaitForSingleObject(_hTPSUpdateEvent, 1000);
}

bool CLanServer::SetWSARecv(st_Session* ptr)
{
	// WSARecv
	WSABUF recvWsa[2];
	int recvRet;
	DWORD flags = 0, recvbytes = 0;
	ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	if (ptr->recvBuf->DirectEnqueueSize() < ptr->recvBuf->GetFreeSize()) 
	{
		// µÎ°³·Î ³ª´² ¹Þ¾Æ¾ß ÇÔ
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
			if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
			{
				// ¿¬°á ²÷±â
				ReleaseSession(ptr);
				return false;
			}
		}
	}

	return true;
}

bool CLanServer::RecvProc(st_Session* ptr, DWORD cbTransferred)
{
	st_NetHeader header;
	char tempBuffer[PROTOCOL_MAX_SIZE + 1];
	CPacket* csPacket = new CPacket(PROTOCOL_MAX_SIZE);

	// ¹ÞÀº µ¥ÀÌÅÍ Ä«ÇÇ
	ptr->recvBuf->MoveRear(cbTransferred);

	int sum = 0;
	// ¹ÞÀº µ¥ÀÌÅÍ¸¦ ÀüºÎ ¼ö½Å ¸µ¹öÆÛ¿¡¼­ »©¸é¼­ OnRecvÈ£­„
	while (1)
	{
		{
			//Profiler pro(L"RecvPro_loop");
			int useSize = ptr->recvBuf->GetUseSize();
			if (useSize < sizeof(st_NetHeader))
			{
				break;
			}

			int peekRet = ptr->recvBuf->Peek((char*)&header, sizeof(st_NetHeader));
			if (peekRet != sizeof(st_NetHeader))
			{
				DebugBreak();
				Disconnect(ptr->ulSessionID);
				return false;
			}

			if (ptr->recvBuf->GetUseSize() < header.shLen + sizeof(st_NetHeader))
			{
				break;
			}

			ptr->recvBuf->MoveFront(sizeof(st_NetHeader));
			int dequeueRet = ptr->recvBuf->Dequeue(csPacket->GetBufferPtr(), header.shLen);
			if (dequeueRet != header.shLen)
			{
				DebugBreak();
				Disconnect(ptr->ulSessionID);
				return false;
			}

			csPacket->MoveWritePos(header.shLen);
			OnRecv(ptr->ulSessionID, csPacket);
			csPacket->Clear();
			InterlockedIncrement((unsigned int*)&_iRecvMessageTPS);
		}
	}

	return true;
}

bool CLanServer::SetWSASend(st_Session* ptr)
{
	int retval;
	DWORD sendbytes;

	InterlockedIncrement((ULONGLONG*)&(ptr->dwIOCount));
	int sendSize = ptr->sendBuf->GetUseSize();
	// WSASend
	if (ptr->sendBuf->DirectDequeueSize() < sendSize)
	{
		// µÎ°³·Î ³ª´² º¸³»¾ßÇÔ
		WSABUF sendWsa[2];
		sendWsa[0].buf = ptr->sendBuf->GetFrontBufferPtr();
		sendWsa[0].len = ptr->sendBuf->DirectDequeueSize();

		sendWsa[1].buf = ptr->sendBuf->GetArrPtr();
		sendWsa[1].len = sendSize - ptr->sendBuf->DirectDequeueSize();
		retval = WSASend(ptr->sock, sendWsa, 2, &sendbytes,
			0, &(ptr->sendOverlapped), NULL);
	}
	else
	{
		WSABUF sendWsa;
		sendWsa.buf = ptr->sendBuf->GetFrontBufferPtr();
		sendWsa.len = sendSize;
		retval = WSASend(ptr->sock, &sendWsa, 1, &sendbytes,
			0, &(ptr->sendOverlapped), NULL);
	}

	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			return false;
		}
	}

	return true;
}

void CLanServer::ReleaseSession(st_Session* ptr)
{
	//Profiler pro(L"ReleaseSession");
	closesocket(ptr->sock);

	OnRelease(ptr->ulSessionID);
	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf->ClearBuffer();
	ptr->bSessionUsing = false;
}