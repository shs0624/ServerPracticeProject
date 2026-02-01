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

	//InitializeSRWLock(&_UserMapLock);
	//InitializeSRWLock(&_SessionMapLock);

	_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	_hTimeoutEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

	InitRoom();

	_UserPool = new procademy::CMemoryPool_LockFree<st_USER>(maxConnection, false, false);
	_SessionPool = new procademy::CMemoryPool_LockFree<st_SESSION>(maxConnection, false, false);

	//_TimerThreadHandle = (HANDLE)_beginthreadex(NULL, 0, TimerThread, this, 0, &_TimerThreadID);

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
	// @@TODO : AUTH에 실질적으로 넣기 전에, 연결이 끊어지면 어떻게하는가
	//pSession->ulSessionID = sessionID;
	//pSession->ClientAddr = clientAddr;
	//pSession->dwLastRecvTime = timeGetTime();

	//AcquireSRWLockExclusive(&_SessionMapLock);
	//_SessionMap.insert({ sessionID, pSession });
	//ReleaseSRWLockExclusive(&_SessionMapLock);

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

	auto it = _RoomMap.find(ptr->dwIncludedRoom);
	if (it == _RoomMap.end())
		DebugBreak();

	stRoomMessage* pMessage = _MessagePool.Alloc();
	pMessage->sessionID = sessionID;
	pMessage->type = RoomMessageType::MESSAGE;
	pMessage->cPacket = cpacket;

	((*it).second)->EnqueueMessage(pMessage);

	_pLog._dwRecvMessageTPS++;
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

void RoomNetServer::mpRESLogin(RefCountPointer& cPacket, BYTE status, ULONGLONG accountNum)
{
	WORD type = en_PACKET_CS_GAME_RES_LOGIN;

	(**cPacket) << type;
	(**cPacket) << status;
	(**cPacket) << accountNum;
}

void RoomNetServer::mpRESEcho(RefCountPointer& cPacket, ULONGLONG accountNum, LONGLONG sendTick)
{
	WORD type = en_PACKET_CS_GAME_RES_ECHO;

	(**cPacket) << type;
	(**cPacket) << accountNum;
	(**cPacket) << sendTick;
}