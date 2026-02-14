#include "Includes.h"
#include "ContentsDefine.h"
#include "NetServer_Pipe.h"
#include "RoomNetServer.h"
#include "IRoom.h"
#include "AuthRoom.h"
#include "EchoRoom.h"
#include "IRoomFactory.h"

// 빌드에러 방지를 위한 정의
thread_local stChatLog IRoom::_pLog;

st_USER* IRoom::AllocUSER() { return _pRoomNetServer->AllocUSER(); }
void IRoom::FreeUSER(st_USER* pUser) { _pRoomNetServer->FreeUSER(pUser); }
st_SESSION* IRoom::AllocSESSION() { return _pRoomNetServer->AllocSESSION(); }
void IRoom::FreeSESSION(st_SESSION* pSession) { _pRoomNetServer->FreeSESSION(pSession); }

void IRoom::SetRoomInfo(DWORD roomNumber, RoomNetServer* pRoomNetServer)
{
	_dwFrameTime = 1000 / dfFRAME;
	_dwRoomNumber = roomNumber;

	_pRoomNetServer = pRoomNetServer;
	_pNetServer = (CNetServer*)pRoomNetServer;

	_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	_MessageQueue = new LockFreeQueue<RefCountPointer>();
}

void IRoom::RegisterLog()
{
	LogController::GetInstance()->RegisterLogStruct(&_pLog);
}