#include "Includes.h"
#include "ContentsDefine.h"
#include "CommonProtocol.h"
#include "NetServer_Pipe.h"
#include "RoomNetServer.h"
#include "IRoom.h"
#include "AuthRoom.h"

void AuthRoom::OnEnter(ULONGLONG sessionID)
{
	EnterAuthRoom(sessionID);
}

void AuthRoom::OnLeave(ULONGLONG sessionID)
{
	LeaveAuthRoom(sessionID);
}

void AuthRoom::AuthProc(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	WORD type;
	ULONGLONG accountNum;
	char sessionKey[64];

	(**cPacket) >> type;
	if (type != en_PACKET_CS_GAME_REQ_LOGIN)
	{
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
}

void AuthRoom::EnterAuthRoom(ULONGLONG sessionID)
{
	auto it = _SessionMap.find(sessionID);
	if (it != _SessionMap.end())
	{
		return;
	}

	// Accept에서 설정한 세션 정보 얻어오기
	st_SESSION* pSession = NULL;
	if (!_pRoomNetServer->GetPTRFromSession(sessionID, (LPVOID*)&pSession))
	{
		return;
	}

	_SessionMap.insert({ sessionID, pSession });

	_pRoomLog._dwAuthUserCount++;
}

void AuthRoom::LeaveAuthRoom(ULONGLONG sessionID)
{
	st_SESSION* pSession = NULL;
	auto it = _SessionMap.find(sessionID);
	if (it == _SessionMap.end())
		return;

	pSession = (*it).second;

	_pRoomLog._dwAuthUserCount--;
	_SessionMap.erase(sessionID);

	if (pSession != NULL)
		FreeSESSION(pSession);
}
