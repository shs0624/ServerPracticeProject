#pragma comment(lib,"ws2_32")
#include <winsock2.h>
#include <WS2tcpip.h>
#include <process.h>
#include <tchar.h>
#include <conio.h>
#include <stdio.h>
#include <unordered_map>
#include "CRingBuffer.h"
#include "IOCPEcho_header.h"
using namespace std;

// IO스레드에서 accept 진행
// Send 스레드
// Recv 스레드 생성

bool b_sendFlag = false;

SOCKET listen_sock;

HANDLE _acceptThreadHandle;
HANDLE _iocpHandle;
HANDLE _iocpWorkerThreadHandleArr[50];

DWORD _threadID = 0;

unsigned int _acceptThreadID;
unsigned int _iocpWorkerThreadID[50];

unordered_map<DWORD, st_Session*> _sessionList;

bool RecvProc(st_Session* ptr, DWORD cbTransferred);
bool SendProc(st_Session* ptr, DWORD cbTransferred);

// 이미 도착한 메시지들에서 문제가 발생
int main(int argc, char* argv[])
{
	int retval;
	InitializeSRWLock(&_srwLock);

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
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

	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++)
	{
		_iocpWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, 0, 0, &_iocpWorkerThreadID[i]);
		if (_iocpWorkerThreadHandleArr[i] == NULL) 
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

		//printf("\n[TCP 서버] 클라이언트 접속 : IP주소 = %s, 포트 번호 = %d\n",
		//	inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));


		// 비동기 입출력 시작
		st_Session* ptr = new st_Session;
		if (ptr == NULL) break;

		// 소켓을 IOCP에 등록
		CreateIoCompletionPort((HANDLE)client_sock, _iocpHandle, (ULONG_PTR)ptr, 0);

		ZeroMemory(&ptr->recvOverlapped, sizeof(ptr->recvOverlapped));
		ZeroMemory(&ptr->sendOverlapped, sizeof(ptr->sendOverlapped));
		ptr->dwSessionID = _threadID++;
		ptr->dwSendCount = 0;
		ptr->sock = client_sock;
		ptr->recvBuf = new CRingBuffer(15000);
		ptr->sendBuf = new CRingBuffer(15000);

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
		//printf("\n[TCP Accept WSARecv] IP주소 = %s, 포트 번호 = %d | recvRet : %d\n",
		//	inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), recvRet);
	}

	return 0;
}

unsigned int WINAPI IOCPWorkerThread(LPVOID arg)
{
	char ipbuffer[50];
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;

	while (1)
	{
		WSABUF wsabuf;
		DWORD cbTransferred, recvbytes;
		SOCKET client_sock;
		st_Session* ptr;
		OVERLAPPED* pOverlapped;
		retval = GetQueuedCompletionStatus(_iocpHandle, &cbTransferred,
			(PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(ptr->sock, (SOCKADDR*)&clientaddr, &addrlen);

		if (retval == 0 || cbTransferred == 0)
		{
			if (ptr->dwSendCount != 0)
			{
				ptr->dwSendCount--;
				continue;
			}

			AcquireSRWLockExclusive(&_srwLock);
			_sessionList.erase(ptr->dwSessionID);
			ReleaseSRWLockExclusive(&_srwLock);

			closesocket(ptr->sock);
			//printf("[TCP 서버] 클라이언트 종료 : IP주소 = %s, 포트 번호 = %d\n",
			//	inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port));
			delete (ptr->recvBuf);
			delete (ptr->sendBuf);
			//printf("[TCP 서버] delete : %p | sendCount : %d\n", ptr, ptr->dwSendCount);
			delete ptr;

			continue;
		}

		if (&(ptr->recvOverlapped) == pOverlapped)
		{
			if (!RecvProc(ptr, cbTransferred))
			{
				continue;
			}

			if (!SendProc(ptr, cbTransferred))
			{
				continue;
			}

			// WSARecv
			WSABUF recvWsa[2];
			int recvRet;
			DWORD flags = 0;
			ZeroMemory(&(ptr->recvOverlapped), sizeof(ptr->recvOverlapped));
			ZeroMemory(&(ptr->sendOverlapped), sizeof(ptr->sendOverlapped));
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
					err_display("WSARecv()_IOWorkerThread");
					continue;
				}
			}

			//printf("\n[TCP WSARecv] IP주소 = %s, 포트 번호 = %d | recvRet : %d\n",
			//	inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50), ntohs(clientaddr.sin_port), recvRet);
		}
		else
		{
			ptr->dwSendCount--;
		}
	}
}

bool RecvProc(st_Session* ptr, DWORD cbTransferred)
{
	char tempBuffer[PROTOCOL_SIZE + 1];

	// 받은 데이터 카피
	ptr->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터를 전부 링버퍼에 넣고, 완성된 패킷들을 읽으며 Send링버퍼에 Enqueue
	while (1)
	{
		if (ptr->recvBuf->GetUseSize() < PROTOCOL_SIZE)
			break;

		int dequeueRet = ptr->recvBuf->Dequeue(tempBuffer, PROTOCOL_SIZE);
		if (dequeueRet != PROTOCOL_SIZE)
		{
			DebugBreak();

			AcquireSRWLockExclusive(&_srwLock);
			_sessionList.erase(ptr->dwSessionID);
			ReleaseSRWLockExclusive(&_srwLock);

			closesocket(ptr->sock);
			delete (ptr->recvBuf);
			delete (ptr->sendBuf);
			delete ptr;
			return false;
		}

		//printf("[TCP / %s : %d] %lld\n", inet_ntop(AF_INET, &(clientaddr.sin_addr), ipbuffer, 50),
		//	ntohs(clientaddr.sin_port), (LONGLONG)*(tempBuffer + sizeof(short)));

		// sendQ 인큐
		int enqueueRet = ptr->sendBuf->Enqueue(tempBuffer, PROTOCOL_SIZE);
		if (enqueueRet != PROTOCOL_SIZE)
		{
			DebugBreak();

			AcquireSRWLockExclusive(&_srwLock);
			_sessionList.erase(ptr->dwSessionID);
			ReleaseSRWLockExclusive(&_srwLock);

			closesocket(ptr->sock);
			delete (ptr->recvBuf);
			delete (ptr->sendBuf);
			delete ptr;
			return false;
		}
	}

	return true;
}

bool SendProc(st_Session* ptr, DWORD cbTransferred)
{
	int retval;
	DWORD sendbytes;

	// WSASend
	if (ptr->sendBuf->DirectDequeueSize() < ptr->sendBuf->GetUseSize())
	{
		// 두개로 나눠 보내야함
		WSABUF sendWsa[2];
		sendWsa[0].buf = ptr->sendBuf->GetFrontBufferPtr();
		sendWsa[0].len = ptr->sendBuf->DirectDequeueSize();

		sendWsa[1].buf = ptr->sendBuf->GetArrPtr();
		sendWsa[1].len = ptr->sendBuf->GetUseSize() - ptr->sendBuf->DirectDequeueSize();
		retval = WSASend(ptr->sock, sendWsa, 2, &sendbytes,
			0, &(ptr->sendOverlapped), NULL);
	}
	else
	{
		WSABUF sendWsa;
		sendWsa.buf = ptr->sendBuf->GetFrontBufferPtr();
		sendWsa.len = ptr->sendBuf->GetUseSize();
		retval = WSASend(ptr->sock, &sendWsa, 1, &sendbytes,
			0, &(ptr->sendOverlapped), NULL);
	}

	ptr->sendBuf->MoveFront(cbTransferred);
	ptr->dwSendCount++;

	if (retval == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			//err_display("WSASend()");
			return false;
		}
	}

	return true;
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