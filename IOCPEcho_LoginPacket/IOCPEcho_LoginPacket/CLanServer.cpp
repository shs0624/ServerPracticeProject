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
#include <deque>
#include <minidumpapiset.h>
#include "CCrashDump.h"

procademy::CCrashDump cCrashDump;
SOCKET listen_sock;

HANDLE _tpsThreadHandle;
HANDLE _acceptThreadHandle;
HANDLE _iocpHandle;
HANDLE _iocpWorkerThreadHandleArr[50];

CRITICAL_SECTION _csIndexStackCS;
CRITICAL_SECTION _csProfilerCS;

ULONGLONG _GetSessionPerSec;

unsigned int _tpsThreadID;
unsigned int _acceptThreadID;
unsigned int _iocpWorkerThreadID[50];

bool _bServerEnabled = true;

bool CLanServer::Start(ULONG ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, WORD maxConnection)
{
	int retval;

	InitializeSessions(maxConnection);

	// ¿©º” √ ±‚»≠
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

	printf("\n[TCP º≠πˆ] Ω√¿€\n");
}

void CLanServer::InitializeSessions(WORD maxConnection)
{
	_iSessionCount = 0;
	_bServerEnabled = true;
	_imaxConnection = maxConnection;

	InitializeCriticalSection(&_csIndexStackCS);
	InitializeCriticalSection(&_csProfilerCS);

	_sessionArr = (st_Session*)malloc(sizeof(st_Session) * maxConnection);

	for (ULONGLONG i = 0; i < _imaxConnection; i++)
	{
		ZeroMemory(&_sessionArr[i].recvOverlapped, sizeof(_sessionArr[i].recvOverlapped));
		ZeroMemory(&_sessionArr[i].sendOverlapped, sizeof(_sessionArr[i].sendOverlapped));
		_sessionArr[i].bSendFlag = false;
		_sessionArr[i].bSessionUsing = false;
		_sessionArr[i].recvBuf = new CRingBuffer(15000);
		InitializeCriticalSection(&_sessionArr[i].crtLock);

		// ¿Œµ¶Ω∫∏∏ º≥¡§«œ∞Ì thraedID¥¬ acceptø°º≠. acceptµµ ºˆ¡§«œ±‚
		_indexStack.push(i);
	}
}

unsigned int WINAPI CLanServer::AcceptThread(LPVOID arg)
{
	// static º±æ«ÿº≠ «‘ºˆ »£√‚¿ª ¿ß«— ∆˜¿Œ≈Õ
	CLanServer* thisPtr = (CLanServer*)arg;

	// µ•¿Ã≈Õ ≈ÎΩ≈ø° ªÁøÎ«“ ∫Øºˆ
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
	// µ•¿Ã≈Õ ≈ÎΩ≈ø° ªÁøÎ«“ ∫Øºˆ
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	ULONGLONG sessionID = 0;

	addrlen = sizeof(clientaddr);
	client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
	if (client_sock == INVALID_SOCKET)
	{
		err_display("accept()");
		return false;
	}
	{
		Profiler("AcceptProc");
		if (!thisPtr->OnConnectionRequest(clientaddr.sin_addr.S_un.S_addr, clientaddr.sin_port))
		{
			return false;
		}

		// ªÁøÎ æ»«œ¥¬ ººº« √£æ∆º≠ µÓ∑œ

		ULONGLONG index;
#ifdef CHECKPROFILE
		EnterCriticalSection(&_csProfilerCS);
#endif
		{
			EnterCriticalSection(&_csIndexStackCS);
			{
#ifdef CHECKPROFILE
				Profiler("GetIndex");
#endif
				index = _indexStack.top();
				_indexStack.pop();
			}
			LeaveCriticalSection(&_csIndexStackCS);

			if (!_sessionArr[index].bSessionUsing)
			{
				_sessionArr[index].bSessionUsing = true;
				ZeroMemory(&_sessionArr[index].recvOverlapped, sizeof(_sessionArr[index].recvOverlapped));
				ZeroMemory(&_sessionArr[index].sendOverlapped, sizeof(_sessionArr[index].sendOverlapped));
				_sessionArr[index].dwIOCount = 0;
				_sessionArr[index].bSendFlag = false;
				_sessionArr[index].sock = client_sock;
				_sessionArr[index].recvBuf->ClearBuffer();
				_sessionArr[index].sendBuf.clear();

				ULONGLONG id = (_threadID++) & 0x0000ffffffffffff;
				ULONGLONG idx = (index << 48);
				_sessionArr[index].ulSessionID = (idx | id);
				sessionID = _sessionArr[index].ulSessionID;

				// º“ƒœ¿ª IOCPø° µÓ∑œ
				CreateIoCompletionPort((HANDLE)client_sock, _iocpHandle, (ULONG_PTR)&_sessionArr[index], 0);
			}
			else
			{
 				DebugBreak();
			}
		}
#ifdef CHECKPROFILE
		LeaveCriticalSection(&_csProfilerCS);
#endif

		thisPtr->OnAccept(sessionID);
		InterlockedIncrement((LONG*)&_iSessionCount);
		InterlockedIncrement((unsigned int*)&_iAcceptTPS);

		if (!SetWSARecv(&_sessionArr[index]))
		{
			if (InterlockedDecrement((DWORD*)&(_sessionArr[index].dwIOCount)) == 0)
			{
				// ø¨∞· ≤˜±‚
				ReleaseSession(&_sessionArr[index]);
				return false;
			}
		}
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
			// ¡æ∑·
			printf("IOCP Worker Thread Exit\n");
			break;
		}

		if (retval == 0 || cbTransferred == 0)
		{
			if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
			{
				// ø¨∞· ≤˜±‚
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
						// ø¨∞· ≤˜±‚
						thisPtr->ReleaseSession(ptr);
					}
					continue;
				}

				// WSARecv
				if (!(thisPtr->SetWSARecv(ptr)))
				{
					if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
					{
						// ø¨∞· ≤˜±‚
						thisPtr->ReleaseSession(ptr);
					}
				}
			}
		}
		else
		{
			//PRO_BEGIN("SendOverlapped");
			EnterCriticalSection(&ptr->crtLock);
			if (ptr->sendBuf.size() < ptr->dwSendCount)
			{
				err_display("SendCount != resultBuf Count");
				thisPtr->Disconnect(ptr->ulSessionID);
				LeaveCriticalSection(&ptr->crtLock);
				continue;
			}

			int loopCnt = ptr->dwSendCount;
			for (int i = 0; i < loopCnt; i++)
			{
				ptr->sendBuf.pop_front();
			}

			ptr->dwSendCount -= loopCnt;
			if (ptr->dwSendCount < 0)
				DebugBreak();

			if (!ptr->sendBuf.empty())
			{
				if (!thisPtr->SetWSASend(ptr))
				{
					InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
					if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
					{
						// ø¨∞· ≤˜±‚
						thisPtr->ReleaseSession(ptr);
					}
				}
			}
			else
			{
				InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
			}
			LeaveCriticalSection(&ptr->crtLock);
			//PRO_END("SendOverlapped");
		}

		if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
		{
			// ø¨∞· ≤˜±‚
			thisPtr->ReleaseSession(ptr);
		}
	}

	return 1;
}

void CLanServer::QuitServer()
{
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

bool CLanServer::Disconnect(ULONGLONG sessionID)
{
	st_Session* pSession = NULL;
#ifdef CHECKPROFILE
	EnterCriticalSection(&_csProfilerCS);
#endif
	{
		GetSession(sessionID, &pSession);
		if (pSession == NULL)
		{
#ifdef CHECKPROFILE
			LeaveCriticalSection(&_csProfilerCS);
#endif
			return false;
		}
	}
#ifdef CHECKPROFILE
	LeaveCriticalSection(&_csProfilerCS);
#endif
	closesocket(pSession->sock);

	return true;
}

bool CLanServer::SendPacket(ULONGLONG sessionID, RefCountPointer<CPacket> cPacket)
{
	st_Session* pSession = NULL;
	{
#ifdef CHECKPROFILE
		EnterCriticalSection(&_csProfilerCS);
		Profiler("GetSession");
#endif
		GetSession(sessionID, &pSession);
		if (pSession == NULL)
		{
#ifdef CHECKPROFILE
			LeaveCriticalSection(&_csProfilerCS);
#endif
			return false;
		}
#ifdef CHECKPROFILE
		LeaveCriticalSection(&_csProfilerCS);
#endif
	}


	short shSize = (*cPacket)->GetDataSize();
	st_NetHeader header;
	header.shLen = shSize;
	
	EnterCriticalSection(&pSession->crtLock);
	(*cPacket)->PushHeader((char*)&header, sizeof(st_NetHeader));
	pSession->sendBuf.push_back(cPacket);
	if (InterlockedExchange((LONG*)&(pSession->bSendFlag), TRUE) != TRUE)
	{
		if (!SetWSASend(pSession))
		{
			LeaveCriticalSection(&pSession->crtLock);
			InterlockedExchange((LONG*)&(pSession->bSendFlag), FALSE);
			if (InterlockedDecrement((DWORD*)&(pSession->dwIOCount)) == 0)
			{
				// ø¨∞· ≤˜±‚
				ReleaseSession(pSession);
			}
			return false;
		}
	}
	LeaveCriticalSection(&pSession->crtLock);

	InterlockedIncrement((unsigned int*) & _iSendMessageTPS);

	return true;
}

void CLanServer::GetSession(ULONGLONG ulSessionID, st_Session** pSession)
{
	{
#ifdef CHECKPROFILE
		Profiler("FindIdx");
#endif
		ULONGLONG idx = (ulSessionID) >> 48;
		*pSession = &_sessionArr[idx];
	}
	InterlockedIncrement((ULONGLONG*)&_GetSessionPerSec);
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
	printf("_iSessionCount : %d # _iReleaseTPS : %d # _iAcceptTPS : %d\n", _iSessionCount, _iReleaseTPS, _iAcceptTPS);

	_iAcceptTPS = 0;
	_iRecvMessageTPS = 0;
	_iSendMessageTPS = 0;
	_iReleaseTPS = 0;
	_GetSessionPerSec = 0;

	WaitForSingleObject(_hTPSUpdateEvent, 1000);
}

bool CLanServer::SetWSARecv(st_Session* ptr)
{
	// WSARecv
#ifdef CHECKPROFILE
	EnterCriticalSection(&_csProfilerCS);
	Profiler("SetWSARecv");
#endif
	WSABUF recvWsa[2];
	int recvRet;
	DWORD flags = 0, recvbytes = 0;
	ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
	if (ptr->recvBuf->DirectEnqueueSize() < ptr->recvBuf->GetFreeSize()) 
	{
		// µŒ∞≥∑Œ ≥™¥≤ πﬁæ∆æﬂ «‘
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
			if (WSAGetLastError() != 0)
			{
#ifdef CHECKPROFILE
				LeaveCriticalSection(&_csProfilerCS);
#endif
				return false;
			}
		}
	}

#ifdef CHECKPROFILE
	LeaveCriticalSection(&_csProfilerCS);
#endif 
	return true;
}

bool CLanServer::RecvProc(st_Session* ptr, DWORD cbTransferred)
{
	st_NetHeader header;
	char tempBuffer[PROTOCOL_MAX_SIZE + 1];

	// πﬁ¿∫ µ•¿Ã≈Õ ƒ´««
	ptr->recvBuf->MoveRear(cbTransferred);

	int sum = 0;
	// πﬁ¿∫ µ•¿Ã≈Õ∏¶ ¿¸∫Œ ºˆΩ≈ ∏µπˆ∆€ø°º≠ ª©∏Èº≠ OnRecv»£≠Ñ
	while (1)
	{
		{
			RefCountPointer<CPacket> csPacket = RefCountPointer<CPacket>::MakeSharedPtr();
			(*csPacket)->Initialize(PROTOCOL_MAX_SIZE + 1, sizeof(st_NetHeader));

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
			int dequeueRet = ptr->recvBuf->Dequeue((*csPacket)->GetTailPtr(), header.shLen);
			if (dequeueRet != header.shLen)
			{
				DebugBreak();
				Disconnect(ptr->ulSessionID);
				return false;
			}

			(*csPacket)->MoveWritePos(header.shLen);
			OnRecv(ptr->ulSessionID, csPacket);
			InterlockedIncrement((unsigned int*)&_iRecvMessageTPS);
		}
	}
	return true;
}

bool CLanServer::SetWSASend(st_Session* ptr)
{
#ifdef CHECKPROFILE
	EnterCriticalSection(&_csProfilerCS);
	Profiler("SetWSASend");
#endif
	int retval, idx = 0;
	DWORD sendbytes;

	InterlockedIncrement((ULONGLONG*)&(ptr->dwIOCount));
	WSABUF sendWsa[SEND_MAX];

	std::deque<void*>::iterator it;
	int size = ptr->sendBuf.size();
	for (int i = 0; i < size; i++)
	{
		CPacket* cPacket = *(ptr->sendBuf[i]);
		sendWsa[i].buf = cPacket->GetHeadPtr();
		sendWsa[i].len = cPacket->GetDataSize();
		idx++;
	}

	retval = WSASend(ptr->sock, sendWsa, idx, &sendbytes,
		0, &(ptr->sendOverlapped), NULL);
	ptr->dwSendCount = idx;

	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			if (WSAGetLastError() != 0)
			{
#ifdef CHECKPROFILE
				LeaveCriticalSection(&_csProfilerCS);
#endif
				return false;
			}
		}
	}

#ifdef CHECKPROFILE
	LeaveCriticalSection(&_csProfilerCS);
#endif
	return true;
}

void CLanServer::ReleaseSession(st_Session* ptr)
{
	EnterCriticalSection(&ptr->crtLock);
	OnRelease(ptr->ulSessionID);
	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf.clear();

	closesocket(ptr->sock);
	ptr->bSessionUsing = false;
	LeaveCriticalSection(&ptr->crtLock);

	InterlockedDecrement((LONG*)&_iSessionCount);

	EnterCriticalSection(&_csIndexStackCS);
	ULONGLONG idx = (ptr->ulSessionID) >> 48;
	_indexStack.push(idx);
	printf("indexStackPush : %d\n", idx);
	LeaveCriticalSection(&_csIndexStackCS);

	InterlockedIncrement((LONG*)&_iReleaseTPS);
}