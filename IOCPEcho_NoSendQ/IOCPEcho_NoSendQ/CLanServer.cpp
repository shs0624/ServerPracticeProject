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
#include <queue>
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
		ULONGLONG index = i << 48;
		//ULONGLONG id = (_threadID++) & 0x0000ffffffffffff;
		_sessionArr[i].ulSessionID = (i << 48);

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

		// ªÁøÎ æ»«œ¥¬ ººº« √£æ∆º≠ µÓ∑œ

		ULONGLONG index;
		EnterCriticalSection(&_csProfilerCS);
		EnterCriticalSection(&_csIndexStackCS);
		{
			{
				Profiler("GetIndex");
				index = _indexStack.top();
				_indexStack.pop();
			}
			LeaveCriticalSection(&_csIndexStackCS);

			if (!_sessionArr[index].bSessionUsing)
			{
				ZeroMemory(&_sessionArr[index].recvOverlapped, sizeof(_sessionArr[index].recvOverlapped));
				ZeroMemory(&_sessionArr[index].sendOverlapped, sizeof(_sessionArr[index].sendOverlapped));
				_sessionArr[index].dwIOCount = 0;
				_sessionArr[index].bSendFlag = false;
				_sessionArr[index].sock = client_sock;
				_sessionArr[index].recvBuf->ClearBuffer();
				while (!_sessionArr[index].sendBuf.empty())
				{
					_sessionArr[index].sendBuf.pop();
				}
				while (!_sessionArr[index].resultBuf.empty())
				{
					_sessionArr[index].resultBuf.pop();
				}

				ULONGLONG id = (_threadID++) & 0x0000ffffffffffff;
				ULONGLONG idx = (index << 48);
				_sessionArr[index].ulSessionID = (idx | id);

				// º“ƒœ¿ª IOCPø° µÓ∑œ
				CreateIoCompletionPort((HANDLE)client_sock, _iocpHandle, (ULONG_PTR)&_sessionArr[index], 0);
				_sessionArr[index].bSessionUsing = true;
			}
			else
			{
				DebugBreak();
			}
		}
		LeaveCriticalSection(&_csProfilerCS);

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
					continue;
				}
			}
		}
		else
		{
			//PRO_BEGIN(L"SendOverlapped");
			EnterCriticalSection(&ptr->crtLock);
			if (ptr->resultBuf.size() != ptr->dwSendCount)
			{
				err_display("SendCount != resultBuf Count");
				thisPtr->Disconnect(ptr->ulSessionID);
				LeaveCriticalSection(&ptr->crtLock);
				continue;
			}

			int loopCnt = ptr->dwSendCount;
			for (int i = 0; i < loopCnt; i++)
			{
				delete(ptr->resultBuf.front());
				ptr->resultBuf.pop();
			}
			printf("dwSendCount : %d # loopCnt : %d\n", ptr->dwSendCount, loopCnt);

			ptr->dwSendCount -= loopCnt;
			if (ptr->dwSendCount < 0)
				DebugBreak();

			if (!ptr->sendBuf.empty())
			{
				thisPtr->SetWSASend(ptr);
			}
			else
			{
				InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
			}
			LeaveCriticalSection(&ptr->crtLock);
			//PRO_END(L"SendOverlapped");
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

bool CLanServer::Disconnect(ULONGLONG sessionID)
{
	st_Session* pSession = NULL;
	EnterCriticalSection(&_csProfilerCS);
	{
		GetSession(sessionID, &pSession);
		if (pSession == NULL)
		{
			LeaveCriticalSection(&_csProfilerCS);
			return false;
		}
	}
	LeaveCriticalSection(&_csProfilerCS);
	closesocket(pSession->sock);

	return true;
}

bool CLanServer::SendPacket(ULONGLONG sessionID, CPacket* cPacket)
{
	CPacket* sendCPacket = new CPacket(PROTOCOL_MAX_SIZE + 1);

	st_Session* pSession = NULL;
	EnterCriticalSection(&_csProfilerCS);
	{
		Profiler("GetSession");
		GetSession(sessionID, &pSession);
		if (pSession == NULL)
			return false;
	}
	LeaveCriticalSection(&_csProfilerCS);

	short shSize = cPacket->GetDataSize();
	st_NetHeader header;
	header.shLen = shSize;
	
	sendCPacket->PutData((char*)&header, sizeof(st_NetHeader));
	cPacket->GetData(sendCPacket->GetBufferPtr() + sizeof(st_NetHeader), shSize);
	sendCPacket->MoveWritePos(shSize);

	EnterCriticalSection(&pSession->crtLock);
	pSession->sendBuf.push(sendCPacket);
	if (InterlockedExchange((LONG*)&(pSession->bSendFlag), TRUE) != TRUE)
	{
		SetWSASend(pSession);
	}
	LeaveCriticalSection(&pSession->crtLock);

	InterlockedIncrement((unsigned int*) & _iSendMessageTPS);

	return true;
}

void CLanServer::GetSession(ULONGLONG ulSessionID, st_Session** pSession)
{
	{
		Profiler("FindIdx");
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
	//printf("GetSessionPerSec : %lld\n", _GetSessionPerSec);

	_iAcceptTPS = 0;
	_iRecvMessageTPS = 0;
	_iSendMessageTPS = 0;
	_GetSessionPerSec = 0;

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
			if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
			{
				// ø¨∞· ≤˜±‚
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
	CPacket csPacket(PROTOCOL_MAX_SIZE);

	// πﬁ¿∫ µ•¿Ã≈Õ ƒ´««
	ptr->recvBuf->MoveRear(cbTransferred);

	int sum = 0;
	// πﬁ¿∫ µ•¿Ã≈Õ∏¶ ¿¸∫Œ ºˆΩ≈ ∏µπˆ∆€ø°º≠ ª©∏Èº≠ OnRecv»£≠Ñ
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
			int dequeueRet = ptr->recvBuf->Dequeue(csPacket.GetBufferPtr(), header.shLen);
			if (dequeueRet != header.shLen)
			{
				DebugBreak();
				Disconnect(ptr->ulSessionID);
				return false;
			}

			csPacket.MoveWritePos(header.shLen);
			OnRecv(ptr->ulSessionID, &csPacket);
			csPacket.Clear();
			InterlockedIncrement((unsigned int*)&_iRecvMessageTPS);
		}
	}

	return true;
}

bool CLanServer::SetWSASend(st_Session* ptr)
{
	int retval, idx = 0;
	DWORD sendbytes;

	InterlockedIncrement((ULONGLONG*)&(ptr->dwIOCount));
	WSABUF sendWsa[SEND_MAX];

	while(!ptr->sendBuf.empty())
	{
		CPacket* cPacket = (CPacket*)ptr->sendBuf.front();
		sendWsa[idx].buf = cPacket->GetBufferPtr();
		ptr->sendBuf.pop();

		// ¥ŸΩ√ ≥÷æÓº≠ ≈•∏¶ √§øÏ±‚
		ptr->resultBuf.push(cPacket);

		sendWsa[idx].len = sizeof(st_NetHeader) + ((st_NetHeader*)sendWsa[idx].buf)->shLen;
		idx++;
	}

	retval = WSASend(ptr->sock, sendWsa, idx, &sendbytes,
		0, &(ptr->sendOverlapped), NULL);
	ptr->dwSendCount = idx;

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
	EnterCriticalSection(&ptr->crtLock);
	LeaveCriticalSection(&ptr->crtLock);
	closesocket(ptr->sock);

	OnRelease(ptr->ulSessionID);
	ptr->recvBuf->ClearBuffer();
	while (!ptr->sendBuf.empty())
	{
		ptr->sendBuf.pop();
	}
	while (!ptr->resultBuf.empty())
	{
		ptr->resultBuf.pop();
	}
	ptr->bSessionUsing = false;

	EnterCriticalSection(&_csIndexStackCS);
	ULONGLONG idx = (ptr->ulSessionID) >> 48;
	_indexStack.push(idx);
	LeaveCriticalSection(&_csIndexStackCS);
}