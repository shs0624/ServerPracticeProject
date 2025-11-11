#pragma once
#pragma comment(lib,"ws2_32")
#include "PacketDefine.h"
#include "Includes.h"
#include "ChatDummy.h"
#include "ChatDummyManager.h"

bool ChatDummyManager::InitManager(string serverIP, int serverPort, int threadCount, int sessionCount)
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		DebugBreak();

	ZeroMemory(&_serverAddr, sizeof(_serverAddr));
	_serverAddr.sin_family = AF_INET;
	if (inet_pton(AF_INET, serverIP.c_str(), &_serverAddr.sin_addr.S_un.S_addr) != 1)
	{
		DebugBreak();
	}
	_serverAddr.sin_port = htons(serverPort);

	_MoveQueue = new LockFreeQueue<ChatDummy*>();
	_ChatQueue = new LockFreeQueue<ChatDummy*>();

	_iThreadCount = threadCount;
	_iSessionCount = sessionCount;

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	int concurrentThread = si.dwNumberOfProcessors - 5;
	if (concurrentThread <= 0)
		concurrentThread = si.dwNumberOfProcessors - 1;

	_IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrentThread);
	if (_IOCPHandle == NULL) return false;

	_hLogUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	_htpsThreadHandle = (HANDLE)_beginthreadex(NULL, 0, LogingThread, this, 0, &_tpsThreadID);
	if (_htpsThreadHandle == NULL)
		return false;

	_hChatDummyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	_hChatDummyThreadHandle = (HANDLE)_beginthreadex(NULL, 0, ChatThread, this, 0, &_ChatDummyThreadID);
	if (_htpsThreadHandle == NULL)
		return false;

	_hMoveDummyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	_hMoveDummyThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MoveThread, this, 0, &_MoveDummyThreadID);
	if (_hMoveDummyThreadHandle == NULL)
		return false;

	//IOCP_THREADCOUNT
	for (int i = 0; i < threadCount; i++)
	{
		_IOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, 0, &_IOCPWorkerThreadID[i]);
		if (_IOCPWorkerThreadHandleArr[i] == NULL)
			return false;
	}

	for (int i = 0; i < 100; i++)
	{
		_DummyArr[i].Init(DummyType::en_Normal, _IOCPHandle);
		PostQueuedCompletionStatus(_IOCPHandle, MAXDWORD, (ULONG_PTR)&_DummyArr[i], _lpWorkOverlapped);
	}
}

unsigned int WINAPI ChatDummyManager::IOCPWorkerThread(LPVOID arg)
{
	char tempBuffer[MAX_PROTOCOLSIZE + 1];
	int retval;
	ChatDummyManager* thisPtr = (ChatDummyManager*)arg;
	srand(time(NULL));

	while (1)
	{
		DWORD cbTransferred = 0;
		ChatDummy* ptr = NULL;
		OVERLAPPED* pOverlapped = NULL;

		retval = GetQueuedCompletionStatus(thisPtr->_IOCPHandle, &cbTransferred, (PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (retval == 0 && ptr == NULL && pOverlapped == NULL)
		{
			// 종료
			return 0;
		}

		if (retval == 0 || cbTransferred == 0)
		{
			InterlockedIncrement(&thisPtr->_dwDisconnectFromServerCount);
			ptr->Disconnect();
			ptr->UpdateAction();
			continue;
		}

		if (pOverlapped == thisPtr->_lpWorkOverlapped)
		{
			// Connect, Disconnect, Login상태는 이 분기를 타야함.
			// @@TODO : 작업 Update, Act는 상속 구조로 바꿔보기
			// @@TODO : Connect가 실패해도 Login으로 넘어감. 애초에 True, False로 이걸 진행하게 하면 안됐음.
			if (!thisPtr->WorkByAction(ptr))
			{
				//ptr->UpdateAction();
				PostQueuedCompletionStatus(thisPtr->_IOCPHandle, MAXDWORD, (ULONG_PTR)ptr, thisPtr->_lpWorkOverlapped);
				continue;
			}

			ptr->UpdateAction();
			continue;
		}
		if (pOverlapped == ptr->GetRecvOverlapped())
		{
			if (!ptr->OnIOCPRecv_RecvProc(cbTransferred, thisPtr->_dwRecvMessageTPS))
			{
				InterlockedIncrement(&thisPtr->_dwResponseFailCount);
				// 디코딩에 에러가 발생 / 메세지가 틀림
			}

			if (!ptr->SetRecv())
			{
				InterlockedIncrement(&thisPtr->_dwDisconnectFromServerCount);
				ptr->Disconnect();
				ptr->UpdateAction();
			}

			if(!ptr->IsWait())
				thisPtr->WorkByAction(ptr);
		}
		else
		{
			if (!ptr->OnIOCPSend(thisPtr->_dwSendMessageTPS))
			{
				InterlockedIncrement(&thisPtr->_dwDisconnectFromServerCount);
				ptr->Disconnect();
				ptr->UpdateAction();
			}
		}

		// @@TODO : Login, Disconnect라면 추가로 PostQueue가 필요하다 .이걸 어떻게 스무스하게 하냐.
		//ptr->UpdateAction();
	}
}

// 세팅된 행동에 따라 적절한 작동 유도
// 모든 작업이 성공한 다음에 호출해야함. 상태를 다음 단계로 바꿔버리기 때문.
bool ChatDummyManager::WorkByAction(ChatDummy* ptr)
{
	DummyAction act = ptr->GetNextAction();
	if (act == DummyAction::en_ActionMove)
	{
		_MoveQueue->Enqueue(ptr);
		SetEvent(_hMoveDummyEvent);
	}
	else if (act == DummyAction::en_ActionChat)
	{
		_ChatQueue->Enqueue(ptr);
		SetEvent(_hChatDummyEvent);
	}
	else if (act == DummyAction::en_ActionLogin)
	{
		ptr->Login();

		// @@TODO : 로그인은 바로 Post하면 안된다. Login 결과가 오기 전까진 작동 안하기 때문이다.
		//PostQueuedCompletionStatus(_IOCPHandle, MAXDWORD, (ULONG_PTR)ptr, _lpWorkOverlapped);
	}
	else if (act == DummyAction::en_ActionConnect)
	{
		InterlockedIncrement(&_dwConnectTry);
		InterlockedIncrement((DWORD*)&_dwConnectWaitCount);
		if (!ptr->Connect(_serverAddr))
		{
			// connect 실패 count 올리기
			InterlockedIncrement(&_dwConnectFail);
			return false;
		}
		InterlockedIncrement(&_dwConnectSuccess);
		InterlockedDecrement((DWORD*)&_dwConnectWaitCount);
	}
	else if (act == DummyAction::en_ActionDisconnect)
	{
		ptr->Disconnect();
		
		PostQueuedCompletionStatus(_IOCPHandle, MAXDWORD, (ULONG_PTR)ptr, _lpWorkOverlapped);
	}
	

	return true;
}

// 로그인이 되면 여기서 어떻게 꺼낼건데? 이건 방법을 바꾸자. 말이 안된다.
// 그냥 로그인하고, LoginWaitCount를 올리자. 그리고 결과가 오면 내리자.
// 만약 못받은 세션은 그대로 잠기는 방향으로 가자.

unsigned int WINAPI ChatDummyManager::MoveThread(LPVOID arg)
{
	ChatDummyManager* thisPtr = (ChatDummyManager*)arg;

	while (1)
	{
		// 락프리 큐에서 큐 사이즈 확인
		short size = thisPtr->_MoveQueue->Size();

		// 사이즈만큼 for문
		for (int i = 0; i < size; i++)
		{
			ChatDummy* ptr;
			thisPtr->_MoveQueue->Dequeue(ptr);

			ptr->Move();
		}

		WaitForSingleObject(thisPtr->_hMoveDummyEvent, TIME_MOVE_EVENT);
	}
}

unsigned int WINAPI ChatDummyManager::ChatThread(LPVOID arg)
{
	ChatDummyManager* thisPtr = (ChatDummyManager*)arg;

	while (1)
	{
		// 락프리 큐에서 큐 사이즈 확인
		short size = thisPtr->_ChatQueue->Size();

		// 사이즈만큼 for문
		for (int i = 0; i < size; i++)
		{
			ChatDummy* ptr;
			thisPtr->_MoveQueue->Dequeue(ptr);

			ptr->Chat();
		}

		WaitForSingleObject(thisPtr->_hChatDummyEvent, TIME_CHAT_EVENT);
	}
}

unsigned int WINAPI ChatDummyManager::LogingThread(LPVOID arg)
{
	ChatDummyManager* thisPtr = (ChatDummyManager*)arg;
	//_setmode(_fileno(stdout), _O_U16TEXT);

	while (1)
	{
		thisPtr->PrintLog();

		// TPS 초기화
		thisPtr->_dwRecvMessageTPS = 0;
		thisPtr->_dwSendMessageTPS = 0;

		WaitForSingleObject(thisPtr->_hLogUpdateEvent, 1000);
	}

	return 0;
}

void ChatDummyManager::PrintLog()
{
	//system("cls");
	char ipBuf[INET_ADDRSTRLEN] = { 0 };
	short port = ntohs(_serverAddr.sin_port);
	inet_ntop(AF_INET, &(_serverAddr.sin_addr), ipBuf, INET_ADDRSTRLEN);

	//_setmode(_fileno(stdout), _O_U16TEXT);

	printf("S : Echo PLAY | Q : Quit\n");
	//wprintf(L"C : Reconnect STOP\n\n");

	printf("====================================================\n");
	printf("Server IP:%-s| Server Port: %-3d\n",
		ipBuf, port);
	printf("====================================================\n");
	printf("Client:%-5d| Thread %-3d\n",
		_iSessionCount, _iThreadCount);
	printf("====================================================\n\n");

	printf("%-25ls%5d\n", L"Thread Loop :", 0);
	printf("%-25ls%5d\n", L"Wait Echo Count :", 0);
	printf("%-25ls%5ls\n", L"Max Latency :", L"0 ms");

	printf("\n%-25ls%5d\n", L"Connect Try :", _dwConnectTry);
	printf("%-25ls%5d\n", L"Connect Success :", _dwConnectSuccess);
	printf("%-25ls%5d\n", L"Connect Fail :", _dwConnectFail);

	printf("\n%-25ls%5d\n", L"Error - Connect Fail :", _dwConnectFail);
	printf("%-25ls%5d\n", L"Error - Disconnect from Server :", _dwDisconnectFromServerCount);

	printf("\n%-25ls%5d\n", L"PacketPool Use :", 0);
	printf("%-25ls%5d\n", L"SendPacket TPS :", _dwSendMessageTPS);
	printf("%-25ls%5d\n", L"RecvPacket TPS :", _dwRecvMessageTPS);
}

