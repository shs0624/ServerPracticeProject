#include "Includes.h"
#include "ContentsDefine.h"
#include "CommonProtocol.h"
#include "NetServer_Pipe.h"
#include "RoomNetServer.h"
#include "IRoom.h"
#include "AuthRoom.h"

void AuthRoom::OnJoin(ULONGLONG sessionID)
{
	// 리스트에 넣어도 될듯.
	st_SESSION* pSession = AllocSESSION();

	if (!_pRoomNetServer->SetPTRToSession(sessionID, pSession, dfROOM_AUTH))
	{
		FreeSESSION(pSession);
		return;
	}

	_SessionMap.insert({ sessionID, pSession });

	_pLog._dwAuthUserCount++;
}

void AuthRoom::OnLeave(ULONGLONG sessionID)
{
	st_SESSION* pSession = NULL;
	auto it = _SessionMap.find(sessionID);
	if (it != _SessionMap.end())
	{
		pSession = (*it).second;

		_pLog._dwAuthUserCount--;
		_SessionMap.erase(sessionID);
	}

	for (int i = 0; i < _SessionVec.size(); i++)
	{
		if (_SessionVec[i]->ulSessionID == sessionID)
		{
			// 있으니까, 제거
			_SessionVec[i] = _SessionVec.back();
			_SessionVec.pop_back();
		}
	}

	if(pSession != NULL)
		FreeSESSION(pSession);
}

void AuthRoom::OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	WORD type;
	ULONGLONG accountNum;
	char sessionKey[64];

	(**cPacket) >> type;
	if (type != en_PACKET_CS_GAME_REQ_LOGIN)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		return;
	}

	(**cPacket) >> accountNum;
	(*cPacket)->GetData(sessionKey, sizeof(sessionKey));

	// Redis 필요?

	st_USER* pUser = AllocUSER();
	pUser->ulSessionID = sessionID;
	pUser->AccountNum = accountNum;
	pUser->dwLastRecvTime = timeGetTime();
	memcpy_s(pUser->SessionKey, sizeof(pUser->SessionKey), sessionKey, sizeof(sessionKey));

	if (!_pRoomNetServer->SetPTRToSession(sessionID, pUser, dfROOM_AUTH))
	{
		FreeUSER(pUser);
		return;
	}

	// MoveRoom도 그냥 방식이 바뀌면 된다. 수정하기
	_pRoomNetServer->MoveRoom(sessionID, _dwRoomNumber, dfROOM_ECHO);
	_pLog._dwLoginMessageTPS++;
}

void AuthRoom::OnUpdate()
{
	// @@TODO : 타이머 체크

	_pLog._dwAuthFPS++;
}

void AuthRoom::OnSessionUpdate(ULONGLONG sessionID)
{
	st_SESSION* pSession = NULL;
	auto it = _SessionMap.find(sessionID);
	if (it == _SessionMap.end())
	{
		pSession = (*it).second;

		_pLog._dwAuthUserCount--;
		_SessionMap.erase(sessionID);
	}

	stRoomMessage* pMessage;
	// 메세지 있는지 체크
	while (!pSession->_MessageQ->Empty())
	{
		pSession->_MessageQ->Dequeue(pMessage);
		if (pMessage == NULL)
			break; // Disconnect?

		RoomMessageType type = pMessage->type;
		switch (type)
		{
		case ENTER:
			OnJoin(pMessage->sessionID);
			break;
		case LEAVE:
			OnLeave(pMessage->sessionID);
			break;
		case MESSAGE:
			OnMessage(pMessage->sessionID, pMessage->cPacket);
			break;
		}
	}

	// Enter,Leave라면 Enter는 입장처리, Leave면 표시 남기기.
}
