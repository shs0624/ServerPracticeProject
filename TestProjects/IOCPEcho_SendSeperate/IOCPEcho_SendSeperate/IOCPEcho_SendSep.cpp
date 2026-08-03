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
#include "IOCPEcho_SendSep.h"
using namespace std;

// IO스레드에서 accept 진행
// Send 스레드
// Recv 스레드 생성

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

unordered_map<DWORD, st_Session*> _sessionList;
procademy::CMemoryPool<st_Session>* _sessionPool;

bool RecvProc(st_Session* ptr, DWORD cbTransferred);
bool SendProc(st_Session* ptr);
void ReleaseSession(st_Session* ptr);

// 이미 도착한 메시지들에서 문제가 발생
int main(int argc, char* argv[])
{
	int retval;
	InitializeSRWLock(&_srwLock);
	InitializeCriticalSection(&_poolLock);
	InitializeCriticalSection(&_echoBufferLock);

	_sessionPool = new procademy::CMemoryPool<st_Session>(200, false, false);
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

	for (int i = 0; i < 3; i++)
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
		ptr->dwIOCount = 1;
		ptr->bSendFlag = false;
		ptr->sock = client_sock;
		ptr->recvBuf = new CRingBuffer(15000);
		ptr->sendBuf = new CRingBuffer(15000);
		InitializeCriticalSection(&(ptr->CrtLock));

		AcquireSRWLockExclusive(&_srwLock);
		_sessionList.insert({ ptr->dwSessionID, ptr });
		ReleaseSRWLockExclusive(&_srwLock);

		WSABUF wsabuf;
		flags = 0;

		wsabuf.buf = ptr->recvBuf->GetRearBufferPtr();
		wsabuf.len = ptr->recvBuf->GetFreeSize();
		int recvRet = WSARecv(ptr->sock, &wsabuf, 1, &recvbytes, &flags, &(ptr->recvOverlapped), NULL);
		if (recvRet == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				err_display("WSARecv()_Accept");
				continue;
			}
		}
	}

	return 0;
}

unsigned int WINAPI EchoThread(LPVOID arg)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;

	while (1)
	{
		WSABUF wsabuf;
		DWORD cbTransferred = 0, recvbytes;
		SOCKET client_sock;
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
		if (_echoBuffer->GetUseSize() < sizeof(st_PACKET_HEADER) + PROTOCOL_SIZE)
		{
			DebugBreak();
			LeaveCriticalSection(&_echoBufferLock);
			continue;
		}

		_echoBuffer->Dequeue((char*)&header, sizeof(st_PACKET_HEADER));

		AcquireSRWLockExclusive(&_srwLock);
		it = _sessionList.find(header.dwSessionID);
		if (it == _sessionList.end())
		{
			DebugBreak();
			ReleaseSRWLockExclusive(&_srwLock);
			LeaveCriticalSection(&_echoBufferLock);
			continue;
		}
		ReleaseSRWLockExclusive(&_srwLock);

		ptr = (*it).second;

		// 세션은 찾았으니, 걔한테 SendPacket
		_echoBuffer->Dequeue(tempBuffer, PROTOCOL_SIZE);
		ptr->sendBuf->Enqueue(tempBuffer, PROTOCOL_SIZE);

		if (InterlockedExchange((ULONGLONG*)&(ptr->bSendFlag), TRUE) != TRUE)
		{
			LeaveCriticalSection(&_echoBufferLock);
			if (!SendProc(ptr))
			{
				if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
				{
					//LeaveCriticalSection(&(ptr->CrtLock));
					// 연결 끊기
					ReleaseSession(ptr);
					continue;
				}
			}
		}
		else
		{
			// 전부 뺐으니까 큐의 끝에 다시 넣어주기?
			header.dwSessionID = ptr->dwSessionID;
			_echoBuffer->Enqueue((char*)&header, sizeof(st_PACKET_HEADER));
			int enqueueRet = _echoBuffer->Enqueue(tempBuffer, PROTOCOL_SIZE);
			if (enqueueRet != PROTOCOL_SIZE)
			{
				DebugBreak();
				ReleaseSession((*it).second);
				LeaveCriticalSection(&_echoBufferLock);
				continue;
			}

			// Post
			PostQueuedCompletionStatus(_EchoIOCPHandle, cbTransferred, (ULONG_PTR)&ptr, &(ptr->sendOverlapped));
			LeaveCriticalSection(&_echoBufferLock);
		}
	}
}

unsigned int WINAPI IOCPWorkerThread(LPVOID arg)
{
	char ipbuffer[50];
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;

	while (1)
	{
		WSABUF wsabuf;
		DWORD cbTransferred = 0, recvbytes;
		SOCKET client_sock;
		st_Session* ptr = NULL;
		OVERLAPPED* pOverlapped;
		retval = GetQueuedCompletionStatus(_NetIOCPHandle, &cbTransferred,
			(PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (pOverlapped == 0 && cbTransferred == 0 && ptr == 0)
		{
			// 종료
			break;
		}

		if (retval == 0 || cbTransferred == 0)
		{
			if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
			{
				// 연결 끊기
				ReleaseSession(ptr);
			}
			continue;
		}

		if (&(ptr->recvOverlapped) == pOverlapped)
		{
			//EnterCriticalSection(&(ptr->CrtLock));
			if (!RecvProc(ptr, cbTransferred))
			{
				if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
				{
					//LeaveCriticalSection(&(ptr->CrtLock));
					// 연결 끊기
					ReleaseSession(ptr);
					continue;
				}
			}

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
			//LeaveCriticalSection(&(ptr->CrtLock));

			if (recvRet == SOCKET_ERROR)
			{
				if (WSAGetLastError() != WSA_IO_PENDING)
				{
					if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
					{
						// 연결 끊기
						ReleaseSession(ptr);
						continue;
					}
				}
			}
		}
		else
		{
			//EnterCriticalSection(&(ptr->CrtLock));
			ptr->sendBuf->MoveFront(cbTransferred);
			int useSize = ptr->sendBuf->GetUseSize();
			if (useSize > 0)
			{
				SendProc(ptr);
			}
			else
			{
				InterlockedExchange((ULONGLONG*)&(ptr->bSendFlag), FALSE);
			}
			//LeaveCriticalSection(&(ptr->CrtLock));
		}

		if (InterlockedDecrement((ULONGLONG*)&(ptr->dwIOCount)) == 0)
		{
			// 연결 끊기
			ReleaseSession(ptr);
		}
	}

	return 1;
}

bool RecvProc(st_Session* ptr, DWORD cbTransferred)
{
	char tempBuffer[PROTOCOL_SIZE + 1];

	st_PACKET_HEADER header;

	// 받은 데이터 카피
	ptr->recvBuf->MoveRear(cbTransferred);

	int sum = 0;
	// 받은 데이터를 전부 수신 링버퍼에서 빼고, 완성된 패킷들을 읽으며 Send링버퍼에 Enqueue
	while (1)
	{
		int useSize = ptr->recvBuf->GetUseSize();
		if (useSize < PROTOCOL_SIZE)
		{
			break;
		}

		int dequeueRet = ptr->recvBuf->Dequeue(tempBuffer, PROTOCOL_SIZE);
		if (dequeueRet != PROTOCOL_SIZE)
		{
			DebugBreak();
			return false;
		}

		header.dwSessionID = ptr->dwSessionID;
		EnterCriticalSection(&_echoBufferLock);
		_echoBuffer->Enqueue((char*)&header, sizeof(st_PACKET_HEADER));
		int enqueueRet = _echoBuffer->Enqueue(tempBuffer, PROTOCOL_SIZE);
		if (enqueueRet != PROTOCOL_SIZE)
		{
			DebugBreak();
			return false;
		}
		LeaveCriticalSection(&_echoBufferLock);

		// Post
		PostQueuedCompletionStatus(_EchoIOCPHandle, cbTransferred, (ULONG_PTR)&ptr, &(ptr->sendOverlapped));
	}

	return true;
}

bool SendProc(st_Session* ptr)
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
	AcquireSRWLockExclusive(&_srwLock);
	_sessionList.erase(ptr->dwSessionID);
	//EnterCriticalSection(&(ptr->CrtLock));
	//LeaveCriticalSection(&(ptr->CrtLock));
	ReleaseSRWLockExclusive(&_srwLock);

	/*
	DeleteCriticalSection(&(ptr->CrtLock));
	closesocket(ptr->sock);

	delete(ptr->recvBuf);
	delete(ptr->sendBuf);
	delete ptr;
	//*/

	closesocket(ptr->sock);
	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf->ClearBuffer();
	//EnterCriticalSection(&_poolLock);
	//_sessionPool->Free(ptr);
	//LeaveCriticalSection(&_poolLock);
	delete(ptr);
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