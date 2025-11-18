#pragma once
#pragma comment(lib,"ws2_32")
#include "Includes.h"
#include "PacketDefine.h"
#include "CommonProtocol.h"
#include "LogController.h"
#include "ChatDummy.h"
#include "DummyHandler.h"
#include "ChatDummyManager.h"
#include "TCPNetwork.h"
#include "ChatDummyController.h"


// 매니저는 모든 스레드의 정보를 통합해서 관리한다.
// 스레드의 첫 시작을 담당하며, 로깅과 오류 정보도 매니저가 가진다.
// 스레드는 ChatDummyController 위주로 작동하게 하자.
bool ChatDummyManager::InitManager(string serverIP, int serverPort, int threadCount, int sessionCountPerThread, bool isTimeoutTest)
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

	_bTestTimeout = isTimeoutTest;
	_iThreadCount = threadCount;
	_iSessionCountPerThread = sessionCountPerThread;	
	_iStartIdx = 0;

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	int concurrentThread = si.dwNumberOfProcessors - 6;
	if (concurrentThread <= 0)
		concurrentThread = si.dwNumberOfProcessors - 1;

	//IOCP_THREADCOUNT
	for (int i = 0; i < 4; i++)
	{
		_hChatDummyThreadHandle[i] = (HANDLE)_beginthreadex(NULL, 0, ChatDummyControlThread, this, 0, &_ChatDummyThreadID[i]);
		if (_hChatDummyThreadHandle[i] == NULL)
			return false;
	}

	LogController::_LogController.Init(_serverAddr, _iSessionCountPerThread * 4, _bTestTimeout);
	// 로그인만 하는 세션과 커넥트만 하는 세션 각각 10개씩.
}

unsigned int WINAPI ChatDummyManager::ChatDummyControlThread(LPVOID arg)
{
	ChatDummyManager* thisPtr = (ChatDummyManager*)arg;
	random_device ran;
	srand(ran());

	bool _bStop = FALSE;

	DummyHandler* dummyHandler = new DummyHandler();
	//dummyHandler->InitHandler(thisPtr->_serverAddr, (thisPtr->_iSessionCountPerThread), 0);
	int startidx = InterlockedExchange((DWORD*)&thisPtr->_iStartIdx, thisPtr->_iStartIdx + dfTHREAD_IDX_JUMPCOUNT);
	dummyHandler->InitHandler(thisPtr->_serverAddr, thisPtr->_iSessionCountPerThread, startidx, thisPtr->_bTestTimeout);

	while (1)
	{
		if (_bStop)
		{
			Sleep(0);
			continue;
		}

		//dummyHandler->Update(thisPtr->Skip());
		if (thisPtr->Skip())
		{
			dummyHandler->Update();
		}
	}
}

bool ChatDummyManager::Skip()
{
	static int iOldTick = timeGetTime();

	int diff = timeGetTime() - iOldTick;
	if (diff < df_FRAMETIME)
	{
		return false;
	}
	else
	{
		iOldTick += df_FRAMETIME;
		return true;
	}
}