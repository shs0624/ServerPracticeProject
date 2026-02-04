#include "Includes.h"
#include "ContentsDefine.h"
#include "NetServer_Pipe.h"
#include "IRoom.h"
#include "AuthRoom.h"
#include "EchoRoom.h"
#include "IRoomFactory.h"
#include "RoomNetServer.h"


TLSMemoryPoolManager<stRoomMessage>
RoomNetServer::_MessagePool(800, 5, 20);

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

	int workCount = (int)si.dwNumberOfProcessors - 2;
	StartNetServer(ip, port, workCount, bNagleEnabled, maxConnection, dfPROGRAM_KEY, dfFIXEDKEY);
}

void RoomNetServer::MoveRoom(ULONGLONG sessionID, DWORD nowRoomNum, DWORD moveRoomNum)
{
	auto nowRoomit = _RoomMap.find(nowRoomNum);
	if (nowRoomit != _RoomMap.end())
	{
		stRoomMessage* pMessage = _MessagePool.Alloc();
		pMessage->sessionID = sessionID;
		pMessage->type = RoomMessageType::LEAVE;

		// 세션, 유저의 해제는 그 스레드에서 하자.
		((*nowRoomit).second)->EnqueueMessage(pMessage);
	}

	auto moveRoomit = _RoomMap.find(moveRoomNum);
	if (moveRoomit != _RoomMap.end())
	{
		stRoomMessage* pMessage = _MessagePool.Alloc();
		pMessage->sessionID = sessionID;
		pMessage->type = RoomMessageType::ENTER;

		// 세션, 유저의 해제는 그 스레드에서 하자.
		((*moveRoomit).second)->EnqueueMessage(pMessage);
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

	stRoomMessage* pMessage = _MessagePool.Alloc();   
	pMessage->sessionID = sessionID;
	pMessage->type = ENTER;

	((*it).second)->EnqueueMessage(pMessage);

	// 세션에 SESSION 구조체 할당
	if (!SetInfoToSession(sessionID, NULL, dfROOM_AUTH))
		return false;

	_pLog._dwSessionCount++;
	return true;
}

void RoomNetServer::OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);

	stRoomMessage* pMessage = _MessagePool.Alloc();
	pMessage->sessionID = sessionID;
	pMessage->type = RoomMessageType::MESSAGE;
	pMessage->cPacket = cpacket;

	ptr->_MessageQ->Enqueue(pMessage);
}

void RoomNetServer::OnRelease(ULONGLONG sessionID)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);

	auto it = _RoomMap.find(ptr->dwIncludedRoom);
	if (it == _RoomMap.end())
		DebugBreak();

	stRoomMessage* pMessage = _MessagePool.Alloc();
	pMessage->sessionID = sessionID;
	pMessage->type = RoomMessageType::LEAVE;

	// 세션, 유저의 해제는 그 스레드에서 하자.
	((*it).second)->EnqueueMessage(pMessage);
}

void RoomNetServer::InitRoom()
{
	// Room 생성
	IRoom* pAuth = IRoomFactory::Create(dfROOM_AUTH);
	pAuth->SetRoomInfo(dfROOM_AUTH, this);
	_RoomMap.insert({ dfROOM_AUTH, pAuth });

	IRoom* pEcho = IRoomFactory::Create(dfROOM_ECHO);
	pEcho->SetRoomInfo(dfROOM_ECHO, this);
	_RoomMap.insert({ dfROOM_ECHO, pEcho });
}