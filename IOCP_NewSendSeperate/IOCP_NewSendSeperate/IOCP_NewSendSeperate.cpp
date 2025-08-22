#pragma comment(lib,"ws2_32")
#include <winsock2.h>
#include <WS2tcpip.h>
#include <process.h>
#include <tchar.h>
#include <conio.h>
#include <stdio.h>
#include <unordered_map>
#include "CRingBuffer.h"
#include "CFreeList.h"
#include "IOCP_NewSendSeperate.h"
using namespace std;

bool b_sendFlag = false;

CRITICAL_SECTION _poolLock;
CRITICAL_SECTION _echoBufferLock;

SOCKET listen_sock;

HANDLE _acceptThreadHandle;
HANDLE _NetIOCPHandle;
HANDLE _NetIOCPWorkerThreadHandleArr[50];

HANDLE _EchoIOCPHandle;
HANDLE _EchoIOCPWorkerThreadHandleArr[3];

DWORD _threadID = 0;

unsigned int _acceptThreadID;
unsigned int _NetIOCPWorkerThreadID[50];
unsigned int _EchoIOCPWorkerThreadID[50];

CRingBuffer* _echoBuffer;

unordered_map<DWORD, st_Session*> _sessionMap;

bool RecvProc(st_Session* ptr, DWORD cbTransferred);

bool SetWSARecv(st_Session* ptr);
bool SetWSASend(st_Session* ptr);
void ReleaseSession(st_Session* ptr);

int main()
{
	int retval;
	InitializeSRWLock(&_sessionMapLock);
	InitializeCriticalSection(&_echoBufferLock);

	_echoBuffer = new CRingBuffer(100000);

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	_NetIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_NetIOCPHandle == NULL) return 1;

	_EchoIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_EchoIOCPHandle == NULL) return 1;

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
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		err_quit("listen()");

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	_acceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, 0, 0, &_acceptThreadID);
	if (_acceptThreadHandle == NULL)
		return 1;

	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2 - 3; i++)
	{
		_NetIOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, 0, 0, &_NetIOCPWorkerThreadID[i]);
		if (_NetIOCPWorkerThreadHandleArr[i] == NULL)
			return 1;
	}

	for (int i = 0; i < 1; i++)
	{
		_EchoIOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, EchoThread, 0, 0, &_EchoIOCPWorkerThreadID[i]);
		if (_EchoIOCPWorkerThreadHandleArr[i] == NULL)
			return 1;
	}

	printf("\n[TCP 서버] 시작\n");
	char ch;
	while (1)
	{
		// 컨트롤?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
			break;
	}

	WSACleanup();
	return 0;
}

unsigned int WINAPI AcceptThread(LPVOID arg)
{
	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	int addrlen;
	DWORD recvbytes, flags;
	char ipbuffer[50];

	while (1)
	{
		//accept()
		addrlen = sizeof(clientaddr);
		client_sock = accept(listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET)
		{
			err_display("accept()");
			continue;
		}

		// 비동기 입출력 시작
		/*EnterCriticalSection(&_poolLock);
		st_Session* ptr = _sessionPool->Alloc();
		LeaveCriticalSection(&_poolLock);*/
		st_Session* ptr = new st_Session;
		if (ptr == NULL) break;

		// 소켓을 IOCP에 등록
		CreateIoCompletionPort((HANDLE)client_sock, _NetIOCPHandle, (ULONG_PTR)ptr, 0);

		// 수정이 필요함
		ZeroMemory(&ptr->recvOverlapped, sizeof(ptr->recvOverlapped));
		ZeroMemory(&ptr->sendOverlapped, sizeof(ptr->sendOverlapped));
		ptr->dwSessionID = _threadID++;
		ptr->dwIOCount = 0;
		ptr->bSendFlag = false;
		ptr->sock = client_sock;
		ptr->recvBuf = new CRingBuffer(15000);
		ptr->sendBuf = new CRingBuffer(15000);
		InitializeCriticalSection(&(ptr->CrtLock));

		AcquireSRWLockExclusive(&_sessionMapLock);
		_sessionMap.insert({ ptr->dwSessionID, ptr });
		ReleaseSRWLockExclusive(&_sessionMapLock);

		SetWSARecv(ptr);
	}

	return 0;
}

unsigned int WINAPI IOCPWorkerThread(LPVOID arg)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;

	while (1)
	{
		DWORD cbTransferred = 0;
		st_Session* ptr = NULL;
		OVERLAPPED* pOverlapped = NULL;

		retval = GetQueuedCompletionStatus(_NetIOCPHandle, &cbTransferred, (PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (retval == 0 && ptr == NULL && pOverlapped == NULL)
		{
			// 종료
			return 0;
		}

		if (retval == 0 && cbTransferred == 0)
		{
			if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
			{
				// 연결 끊기
				ReleaseSession(ptr);
			}
			continue;
		}

		if (pOverlapped == &ptr->recvOverlapped)
		{
			if (!RecvProc(ptr, cbTransferred))
			{
				if (InterlockedDecrement(&ptr->dwIOCount) == 0)
				{
					ReleaseSession(ptr);
					continue;
				}
			}

			if (!SetWSARecv(ptr))
			{
				if (InterlockedDecrement(&(ptr->dwIOCount)) == 0)
				{
					// 연결 끊기
					ReleaseSession(ptr);
					continue;
				}
			}
		}
		else
		{
			EnterCriticalSection(&ptr->CrtLock);
			ptr->sendBuf->MoveFront(cbTransferred);
			int useSize = ptr->sendBuf->GetUseSize();
			if (useSize > 0)
			{
				if (!SetWSASend(ptr))
				{
					if (InterlockedDecrement(&(ptr->dwIOCount)) == 0)
					{
						// 연결 끊기
						LeaveCriticalSection(&ptr->CrtLock);
						ReleaseSession(ptr);
						continue;
					}
				}
			}
			else
			{
				InterlockedExchange((ULONGLONG*)&(ptr->bSendFlag), FALSE);
			}
			LeaveCriticalSection(&ptr->CrtLock);
		}

		if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
		{
			// 연결 끊기
			ReleaseSession(ptr);
		}
	}
}


unsigned int WINAPI EchoThread(LPVOID arg)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;

	while (1)
	{
		WSABUF wsabuf;
		DWORD cbTransferred = 0;
		st_Session* ptr = NULL;
		OVERLAPPED* pOverlapped = NULL;
		unordered_map<DWORD, st_Session*>::iterator it;
		st_PACKET_HEADER header;

		retval = GetQueuedCompletionStatus(_EchoIOCPHandle, &cbTransferred,
			(PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (cbTransferred == 0 && ptr == NULL && pOverlapped == NULL)
		{
			// 종료
			continue;
		}

		EnterCriticalSection(&_echoBufferLock);
		if (_echoBuffer->GetUseSize() < sizeof(st_PACKET_HEADER) + sizeof(st_PACKET))
		{
			DebugBreak();
			LeaveCriticalSection(&_echoBufferLock);
			continue;
		}

		_echoBuffer->Dequeue((char*)&header, sizeof(st_PACKET_HEADER));

		AcquireSRWLockExclusive(&_sessionMapLock);
		it = _sessionMap.find(header.dwSessionID);
		if (it == _sessionMap.end())
		{
			DebugBreak();
			ReleaseSRWLockExclusive(&_sessionMapLock);
			LeaveCriticalSection(&_echoBufferLock);
			continue;
		}

		ptr = (*it).second;
		EnterCriticalSection(&ptr->CrtLock);

		ReleaseSRWLockExclusive(&_sessionMapLock);

		// 세션은 찾았으니, 걔한테 SendPacket
		_echoBuffer->Dequeue(tempBuffer, PROTOCOL_SIZE);
		LeaveCriticalSection(&_echoBufferLock);

		ptr->sendBuf->Enqueue(tempBuffer, PROTOCOL_SIZE);
		if (InterlockedExchange((ULONGLONG*)&(ptr->bSendFlag), TRUE) == FALSE)
		{
			if (!SetWSASend(ptr))
			{
				if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
				{
					// 연결 끊기
					LeaveCriticalSection(&ptr->CrtLock);
					ReleaseSession(ptr);
					continue;
				}
			}
		}
		else
		{
			// send버퍼에 넣어주기
			/*int enqueueRet = ptr->sendBuf->Enqueue(tempBuffer, PROTOCOL_SIZE);
			if (enqueueRet != PROTOCOL_SIZE)
			{
				DebugBreak();
				closesocket(ptr->sock);
				LeaveCriticalSection(&ptr->CrtLock);
				continue;
			}*/			
		}
		LeaveCriticalSection(&ptr->CrtLock);
	}
}

bool RecvProc(st_Session* ptr, DWORD cbTransferred)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	st_PACKET packet;
	st_PACKET_HEADER header;

	ptr->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터 순회하며 Echo버퍼에 넣기
	while (1)
	{
		int useSize = ptr->recvBuf->GetUseSize();
		if (useSize < sizeof(packet.shLen))
		{
			break;
		}

		int peekRet = ptr->recvBuf->Peek((char*)&packet.shLen, sizeof(packet.shLen));
		if (peekRet != sizeof(packet.shLen))
		{
			DebugBreak();
			closesocket(ptr->sock);
			return false;
		}

		if (useSize < sizeof(packet.shLen) + packet.shLen)
		{
			DebugBreak();
			closesocket(ptr->sock);
			return false;
		}

		ptr->recvBuf->MoveFront(sizeof(packet.shLen));
		ptr->recvBuf->Dequeue((char*)&packet.llNum, packet.shLen);

		header.dwSessionID = ptr->dwSessionID;
		EnterCriticalSection(&_echoBufferLock);
		_echoBuffer->Enqueue((char*)&header, sizeof(st_PACKET_HEADER));
		int enqueueRet = _echoBuffer->Enqueue((char*)&packet, sizeof(st_PACKET));
		LeaveCriticalSection(&_echoBufferLock);

		if (enqueueRet != sizeof(st_PACKET))
		{
			DebugBreak();
			closesocket(ptr->sock);
			return false;
		}
		
		// Post
		PostQueuedCompletionStatus(_EchoIOCPHandle, cbTransferred, (ULONG_PTR)&ptr, &(ptr->sendOverlapped));
	}

	return true;
}

bool SetWSARecv(st_Session* ptr)
{
	// WSARecv
	WSABUF recvWsa[2];
	int recvRet;
	DWORD flags = 0, recvbytes = 0;
	ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
	InterlockedIncrement((ULONGLONG*)&(ptr->dwIOCount));
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

bool SetWSASend(st_Session* ptr)
{
	int retval;
	DWORD sendbytes;

	InterlockedIncrement((ULONGLONG*)&(ptr->dwIOCount));
	int sendSize = ptr->sendBuf->GetUseSize();
	// WSASend
	if (ptr->sendBuf->DirectDequeueSize() < sendSize)
	{
		// 두개로 나눠 보내야함
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

void ReleaseSession(st_Session* ptr)
{
	AcquireSRWLockExclusive(&_sessionMapLock);
	_sessionMap.erase(ptr->dwSessionID);
	ReleaseSRWLockExclusive(&_sessionMapLock);

	EnterCriticalSection(&(ptr->CrtLock));
	LeaveCriticalSection(&(ptr->CrtLock));

	closesocket(ptr->sock);
	DeleteCriticalSection(&(ptr->CrtLock));

	delete(ptr->recvBuf);
	delete(ptr->sendBuf);
	delete ptr;

	/*
	closesocket(ptr->sock);
	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf->ClearBuffer();
	//EnterCriticalSection(&_poolLock);
	//_sessionPool->Free(ptr);
	//LeaveCriticalSection(&_poolLock);
	delete(ptr);
	*/
}

// 소켓 함수 오류 출력 후 종료
inline void err_quit(const char* msg)
{
	int err = WSAGetLastError();
	printf("[%s] TCP Error Number : %d\n", msg, err);
	exit(1);
}

inline void err_display(const char* msg)
{
	int err = WSAGetLastError();
	printf("[%s] TCP Error Number : %d\n", msg, err);
	return;
}