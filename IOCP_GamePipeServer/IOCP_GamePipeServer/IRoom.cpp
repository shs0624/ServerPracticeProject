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

bool IRoom::FreeMessage(stRoomMessage* pOutput)
{
	stRoomMessage Message;
	stRoomMessage* ptr = &Message;
	if (!_MessageQueue->Empty())
	{
		_MessageQueue->Dequeue(ptr);

		pOutput->sessionID = ptr->sessionID;
		pOutput->type = ptr->type;
		if(ptr->type == MESSAGE)
			pOutput->cPacket = ptr->cPacket;

		_pRoomNetServer->FreeMessage(ptr);
		return true;
	}
	else
		return false;
}

void IRoom::SetRoomInfo(DWORD roomNumber, RoomNetServer* pRoomNetServer)
{
	_dwFrameTime = 1000 / dfFRAME;
	_dwRoomNumber = roomNumber;

	_pRoomNetServer = pRoomNetServer;
	_pNetServer = (CNetServer*)pRoomNetServer;

	_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	_MessageQueue = new LockFreeQueue<stRoomMessage*>();

	//_RoomThreadHandle = (HANDLE)_beginthreadex(NULL, 0, RoomThread, this, 0, &_RoomThreadID);
}

//void IRoom::UpdateSession()
//{
//	for (int i = 0; i < _SessionVec.size(); i++)
//	{
//		st_NetSession* pSession = _SessionVec[i];
//
//		OnSessionUpdate(pSession->ulSessionID);
//	}
//}

void IRoom::RegisterLog()
{
	LogController::GetInstance()->RegisterLogStruct(&_pLog);
}

bool IRoom::SleepCheck()
{
	int ret = WaitForSingleObject(_hQuitEvent, _dwFrameTime);
	if (ret == WAIT_OBJECT_0)
	{
		// 서버 종료
		return false;
	}

	return true;
}