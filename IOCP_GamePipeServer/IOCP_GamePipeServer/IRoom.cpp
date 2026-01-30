#include "Includes.h"
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

bool IRoom::DequeueMessage(stRoomMessage* pOutput)
{
	stRoomMessage Message;
	stRoomMessage* ptr = &Message;
	if (!_MessageQueue.Empty())
	{
		_MessageQueue.Dequeue(ptr);

		pOutput->cPacket = ptr->cPacket;
		pOutput->sessionID = ptr->sessionID;
		pOutput->type = ptr->type;

		_pRoomNetServer->FreeMessage(ptr);
		return true;
	}
	else
		return false;
}

void IRoom::EnqueueMessage(stRoomMessage* pMessage)
{
	_MessageQueue.Enqueue(pMessage);

	SetEvent(_hThreadEvent);
}

void IRoom::SetRoomInfo(DWORD roomNumber, RoomNetServer* pRoomNetServer)
{
	_dwFrameTime = 1000 / dfFRAME;
	_dwRoomNumber = roomNumber;
	_pRoomNetServer = pRoomNetServer;

	_RoomThreadHandle = (HANDLE)_beginthreadex(NULL, 0, RoomThread, this, 0, &_RoomThreadID);
}