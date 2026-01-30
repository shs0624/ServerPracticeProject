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
	_pLog._dwSessionCount++;
}

void AuthRoom::OnLeave(ULONGLONG sessionID)
{
	auto it = _SessionMap.find(sessionID);
	if (it != _SessionMap.end())
	{
		_SessionMap.erase(it);
	}

	FreeSESSION((*it).second);
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

	_pRoomNetServer->MoveRoom(sessionID, _dwRoomNumber, dfROOM_ECHO);
}