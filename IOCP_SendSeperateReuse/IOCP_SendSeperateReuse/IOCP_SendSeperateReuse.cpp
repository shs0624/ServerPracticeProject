#pragma comment(lib,"ws2_32")
#include <winsock2.h>
#include <WS2tcpip.h>
#include <process.h>
#include <tchar.h>
#include <conio.h>
#include <stdio.h>
#include <unordered_map>
#include "CRingBuffer.h"
#include "IOCP_SendSeperateReuse.h"
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

// 300 사이즈 배열로 동적할당
st_Session* _sessionArr;
long _iSessionCount;

int FindUsableSessionIndex();
int FindSession(int sessionID);
bool Init();
bool RecvProc(st_Session* ptr, DWORD cbTransferred);

bool SetWSARecv(st_Session* ptr);
bool SetWSASend(st_Session* ptr);
void ReleaseSession(st_Session* ptr);

int main()
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

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

	if (!Init())
		return 1;

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

bool Init()
{
	InitializeSRWLock(&_sessionMapLock);
	InitializeCriticalSection(&_echoBufferLock);

	_echoBuffer = new CRingBuffer(100000);
	_sessionArr = (st_Session*)malloc(sizeof(st_Session) * 300);
	_iSessionCount = 0;

	for (int i = 0; i < 300; i++)
	{
		_sessionArr[i].bSessionUsing = false;
		_sessionArr[i].sendBuf = new CRingBuffer(15000);
		_sessionArr[i].recvBuf = new CRingBuffer(15000);
	}

	_NetIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_NetIOCPHandle == NULL) return false;

	_EchoIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_EchoIOCPHandle == NULL) return false;

	_acceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, 0, 0, &_acceptThreadID);
	if (_acceptThreadHandle == NULL)
		return false;

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2 - 1; i++)
	{
		_NetIOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, 0, 0, &_NetIOCPWorkerThreadID[i]);
		if (_NetIOCPWorkerThreadHandleArr[i] == NULL)
			return false;
	}

	for (int i = 0; i < 1; i++)
	{
		_EchoIOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, EchoThread, 0, 0, &_EchoIOCPWorkerThreadID[i]);
		if (_EchoIOCPWorkerThreadHandleArr[i] == NULL)
			return false;
	}
}

int FindUsableSessionIndex()
{
	for (int i = 0; i < 300; i++)
	{
		if (!_sessionArr[i].bSessionUsing)
			return i;
	}

	return -1;
}

int FindSession(int sessionID)
{
	for (int i = 0; i < 300; i++)
	{
		if (_sessionArr[i].dwSessionID == sessionID && _sessionArr[i].bSessionUsing)
			return i;
	}

	return -1;
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

		AcquireSRWLockExclusive(&_sessionMapLock);
		// 비동기 입출력 시작
		int idx = FindUsableSessionIndex();
		if (idx == -1)
		{
			ReleaseSRWLockExclusive(&_sessionMapLock);
			DebugBreak();
		}
		ReleaseSRWLockExclusive(&_sessionMapLock);

		st_Session* ptr = &_sessionArr[idx];

		// 수정이 필요함
		ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
		ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
		ptr->dwSessionID = _threadID++;
		ptr->dwIOCount = 0;
		ptr->bSendFlag = false;
		ptr->sock = client_sock;
		ptr->sendBuf->ClearBuffer();
		ptr->recvBuf->ClearBuffer();
		ptr->bSessionUsing = true;

		InterlockedIncrement((LONG*)&_iSessionCount);

		// 소켓을 IOCP에 등록
		CreateIoCompletionPort((HANDLE)client_sock, _NetIOCPHandle, (ULONG_PTR)ptr, 0);

		if (!SetWSARecv(ptr))
		{
			if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
			{
				// 연결 끊기
				ReleaseSession(ptr);
			}
		}
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
			if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
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
				if (InterlockedDecrement((DWORD*)&ptr->dwIOCount) == 0)
				{
					ReleaseSession(ptr);
					continue;
				}
			}

			if (!SetWSARecv(ptr))
			{
				if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
				{
					// 연결 끊기
					ReleaseSession(ptr);
					continue;
				}
			}
		}
		else
		{
			ptr->sendBuf->MoveFront(cbTransferred);
			int useSize = ptr->sendBuf->GetUseSize();
			if (useSize > 0)
			{
				if (!SetWSASend(ptr))
				{
					if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
					{
						// 연결 끊기
						ReleaseSession(ptr);
					}
				}
			}
			else
			{
				InterlockedExchange((DWORD*)&(ptr->bSendFlag), FALSE);
			}
		}

		if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
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
		_echoBuffer->Dequeue(tempBuffer, PROTOCOL_SIZE);
		LeaveCriticalSection(&_echoBufferLock);

		AcquireSRWLockExclusive(&_sessionMapLock);
		int idx = FindSession(header.dwSessionID);
		if (idx == -1)
		{
			ReleaseSRWLockExclusive(&_sessionMapLock);
			continue;
		}

		ptr = &_sessionArr[idx];
		ReleaseSRWLockExclusive(&_sessionMapLock);
		// 세션은 찾았으니, 걔한테 SendPacket

		ptr->sendBuf->Enqueue(tempBuffer, PROTOCOL_SIZE);
		if (InterlockedExchange((DWORD*)&(ptr->bSendFlag), TRUE) == FALSE)
		{
			if (!SetWSASend(ptr))
			{
				if (InterlockedDecrement((DWORD*)&(ptr->dwIOCount)) == 0)
				{
					// 연결 끊기
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
	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
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

	InterlockedIncrement((DWORD*)&(ptr->dwIOCount));
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
	closesocket(ptr->sock);
	ptr->recvBuf->ClearBuffer();
	ptr->sendBuf->ClearBuffer();
	ptr->bSessionUsing = false;
	InterlockedDecrement((LONG*)&_iSessionCount);
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