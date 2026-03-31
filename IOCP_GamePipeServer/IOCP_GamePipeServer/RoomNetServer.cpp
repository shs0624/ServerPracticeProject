#include "Includes.h"
#include "ContentsDefine.h"
#include "NetServer_Pipe.h"
#include "IRoom.h"
#include "AuthRoom.h"
#include "EchoRoom.h"
#include "IRoomFactory.h"
#include "RoomNetServer.h"

//TLSMemoryPoolManager<stRoomMessage>
//RoomNetServer::_MessagePool(800, 5, 20);

void RoomNetServer::InitRoomNetServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	_hTimeoutEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

	InitRoom();

	// @@TODO: 세션, 유저에 대한 MessageQ 초기화 필요

	_UserPool = new procademy::CMemoryPool_LockFree<st_USER>(maxConnection, false, false);
	_SessionPool = new procademy::CMemoryPool_LockFree<st_SESSION>(maxConnection, false, false);

	//InitPool(maxConnection);

	int workCount = (int)si.dwNumberOfProcessors - 2;
	StartNetServer(ip, port, workCount, bNagleEnabled, maxConnection, dfPROGRAM_KEY, dfFIXEDKEY);
}

void RoomNetServer::MoveRoom(ULONGLONG sessionID, DWORD nowRoomNum, DWORD moveRoomNum)
{
	auto nowRoomit = _RoomMap.find(nowRoomNum);
	if (nowRoomit != _RoomMap.end())
	{
		// 세션, 유저의 해제는 그 스레드에서 하자.
		((*nowRoomit).second)->pRoomPtr->EnqueueEntryMessage(sessionID, LEAVE);
	}

	auto moveRoomit = _RoomMap.find(moveRoomNum);
	if (moveRoomit != _RoomMap.end())
	{
		// 세션, 유저의 해제는 그 스레드에서 하자.
		((*moveRoomit).second)->pRoomPtr->EnqueueEntryMessage(sessionID, ENTER);
	}

	st_NetSession* ptr;
	FindSession(sessionID, &ptr);
	if (ptr != NULL)
	{
		ptr->dwIncludedRoom = moveRoomNum;
	}
}

bool RoomNetServer::OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr)
{
	auto it = _RoomMap.find(dfROOM_AUTH);
	if (it == _RoomMap.end())
		DebugBreak();

	st_SESSION* pSession = AllocSESSION();
	pSession->ulSessionID = sessionID;
	pSession->dwLastRecvTime = timeGetTime();

	if (!SetInfoToSession(sessionID, pSession, dfROOM_AUTH))
	{
		FreeSESSION(pSession);
		return false;
	}

	(*it).second->pRoomPtr->EnqueueEntryMessage(sessionID, ENTER);

	return true;
}

void RoomNetServer::OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);

	ptr->_MessageQ->Enqueue(cpacket);
}

void RoomNetServer::OnRelease(ULONGLONG sessionID)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);

	auto it = _RoomMap.find(ptr->dwIncludedRoom);
	//if (it == _RoomMap.end())
	//	DebugBreak();

	// 세션, 유저의 해제는 그 스레드에서 하자.
	((*it).second)->pRoomPtr->EnqueueEntryMessage(sessionID, LEAVE);
}

void RoomNetServer::InitRoom()
{
	// Room 생성
	RoomInfo* pAuthInfo = new RoomInfo();
	IRoom* pAuth = IRoomFactory::Create(dfROOM_AUTH);
	pAuth->SetRoomInfo(dfROOM_AUTH, this);

	pAuthInfo->pRoomPtr = pAuth;
	pAuthInfo->dwRoomNumber = dfROOM_AUTH;
	_RoomMap.insert({ dfROOM_AUTH, pAuthInfo });
	_AuthRoomThreadHandle = (HANDLE)_beginthreadex(NULL, 0, RoomThread, pAuthInfo, 0, &_AuthRoomThreadID);

	RoomInfo* pEchoInfo = new RoomInfo();
	IRoom* pEcho = IRoomFactory::Create(dfROOM_ECHO);
	pEcho->SetRoomInfo(dfROOM_ECHO, this);

	pEchoInfo->pRoomPtr = pEcho;
	pEchoInfo->dwRoomNumber = dfROOM_ECHO;
	_RoomMap.insert({ dfROOM_ECHO, pEchoInfo });
	_EchoRoomThreadHandle = (HANDLE)_beginthreadex(NULL, 0, RoomThread, pEchoInfo, 0, &_EchoRoomThreadID);
}

bool RoomNetServer::AddSessionToRoom(ULONGLONG sessionID, DWORD roomNumber)
{
	auto it = _RoomMap.find(roomNumber);
	if (it == _RoomMap.end())
		return false;

	st_NetSession* pSession = NULL;
	FindSession(sessionID, &pSession);
	if (pSession == NULL)
		return false;

	if (pSession->bReleaseFlag)
		return false;

	(*it).second->vNetSessionVec.push_back(pSession);
	return true;
}

void RoomNetServer::RemoveSessionFromRoom(ULONGLONG sessionID, DWORD roomNumber)
{
	auto it = _RoomMap.find(roomNumber);
	if (it == _RoomMap.end())
		return;

	st_NetSession* pSession = NULL;
	for (int i = 0; i < (*it).second->vNetSessionVec.size(); i++)
	{
		if ((*it).second->vNetSessionVec[i]->ulSessionID == sessionID)
		{
			pSession = (*it).second->vNetSessionVec[i];

			(*it).second->vNetSessionVec[i] = (*it).second->vNetSessionVec.back();
			(*it).second->vNetSessionVec.pop_back();

			if (pSession->bReleaseCheck)
			{
				//ReleaseSession(sessionID);
				PostRelease(pSession);
			}
			break;
		}
	}
}

unsigned int WINAPI RoomNetServer::RoomThread(LPVOID arg)
{
	RoomInfo* roomPtr = (RoomInfo*)arg;
	IRoom* pIRoom = roomPtr->pRoomPtr;
	DWORD roomNumber = pIRoom->GetRoomNumber();

	vector<st_NetSession*>& pRoomVec = roomPtr->vNetSessionVec;
	pIRoom->RegisterLog();

	DWORD ret = 0;
	while (1)
	{
		pIRoom->UpdateRoom();

		int size = pRoomVec.size();
		for (int i = 0; i < size; i++)
		{
			st_NetSession* ptr = pRoomVec[i];
			ULONGLONG sessionID = ptr->ulSessionID;

			if (ptr->dwIncludedRoom != roomNumber)
				continue;

			// 수신 버퍼에서 메세지를 꺼내 처리
			int qSize = ptr->_MessageQ->GetUseSize();
			for (int j = 0; j < qSize; j++)
			{
				RefCountPointer pMessage;
				if(ptr->_MessageQ->Dequeue(pMessage))
					pIRoom->OnMessage(sessionID, pMessage);
			}

			// 세션별로 Update가 필요하다면 호출하는 OnSessionUpdate
			pIRoom->OnSessionUpdate(sessionID);
		}

		if (!pIRoom->SleepCheck())
			return 0;
	}
}