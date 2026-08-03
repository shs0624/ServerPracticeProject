#include "Includes.h"
#include "ContentsDefine.h"
#include "NetServer_Pipe.h"
#include "RoomNetServer.h"
#include "IRoom.h"
#include "AuthRoom.h"
#include "EchoRoom.h"
#include "IRoomFactory.h"

// 빌드에러 방지를 위한 정의
thread_local stChatLog IRoom::_pRoomLog;

st_USER* IRoom::AllocUSER() { return _pRoomNetServer->AllocUSER(); }
void IRoom::FreeUSER(st_USER* pUser) { _pRoomNetServer->FreeUSER(pUser); }
st_SESSION* IRoom::AllocSESSION() { return _pRoomNetServer->AllocSESSION(); }
void IRoom::FreeSESSION(st_SESSION* pSession) { _pRoomNetServer->FreeSESSION(pSession); }

void IRoom::EnqueueEntryMessage(ULONGLONG sessionID, WORD type)
{
	//RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	RefCountPointer cPacket = RefCountPointer::MakePtr();
	(**cPacket) << sessionID;
	(**cPacket) << type;

	_pRoomLog._dwPacketPoolUse++;

	_MessageQueue->Enqueue(cPacket);
}

void IRoom::EnterRoom(ULONGLONG sessionID)
{
	if (!_pRoomNetServer->AddSessionToRoom(sessionID, _dwRoomNumber))
	{
		return;
	}

	OnEnter(sessionID);
}

void IRoom::LeaveRoom(ULONGLONG sessionID)
{
	_pRoomNetServer->RemoveSessionFromRoom(sessionID, _dwRoomNumber);

	OnLeave(sessionID);
}

void IRoom::SetRoomInfo(DWORD roomNumber, RoomNetServer* pRoomNetServer)
{
	_dwFrameTime = 1000 / dfFRAME;
	_ulNextFrameTick = GetTickCount64() + _dwFrameTime;
	_dwRoomNumber = roomNumber;

	_pRoomNetServer = pRoomNetServer;
	_pNetServer = (CNetServer*)pRoomNetServer;

	_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	_MessageQueue = new LockFreeQueue<RefCountPointer>();
}

void IRoom::RegisterLog()
{
	LogController::GetInstance()->RegisterLogStruct(&_pRoomLog);
}