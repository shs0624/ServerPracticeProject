#pragma once
#include "CommonProtocol.h"
#include "NetServer_Room.h"

class EchoRoom : IRoom
{
public:
	// Enter, Leave 했을 때
	virtual void OnJoin(ULONGLONG sessionID)
	{
		st_USER* pUser = NULL;
		if (!_pRoomNetServer->GetPTRFromSession(sessionID, (LPVOID*)&pUser))
		{
			return;
		}

		auto it = _UserMap.find(sessionID);
		if (it != _UserMap.end())
		{
			// 중복로그인 - sessionID
			_pRoomNetServer->DisconnectSession(sessionID);
			_UserMap.erase(sessionID);

			_pLog._dwAuthUserCount--;
			_pLog._dwSessionCount--;
			return;
		}

		it = _AccountUserMap.find(pUser->AccountNum);
		if (it != _UserMap.end())
		{
			// 중복로그인 - sessionID
			_pRoomNetServer->DisconnectSession(sessionID);

			_pLog._dwGameUserCount--;
			_pLog._dwSessionCount--;
			_pLog._dwDuplicatedLoginTotal++;
		}

		_UserMap.insert({ sessionID, pUser });
		_AccountUserMap.insert({ pUser->AccountNum, pUser });

		_pLog._dwAuthUserCount--;
		_pLog._dwGameUserCount++;
		_pLog._dwSessionCount++;
		_pLog._dwLoginMessageTPS++;

		// RES Send
		RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
		(*cPacket)->Initialize(sizeof(st_NetHeader));
		_pRoomNetServer->mpRESLogin(cPacket, true, pUser->AccountNum);
		_pRoomNetServer->RoomSendPacket(sessionID, cPacket);
	}

	virtual void OnLeave(ULONGLONG sessionID)
	{
		auto it = _UserMap.find(sessionID);
		if (it != _UserMap.end())
		{
			// 중복로그인 - sessionID
			_pRoomNetServer->DisconnectSession(sessionID);

			_pLog._dwAuthUserCount--;
			_pLog._dwSessionCount--;
			return;
		}

	}

	virtual void OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket)
	{

	}

	virtual void OnUpdate()
	{

	}
private:
	unordered_map<ULONGLONG, st_USER*> _UserMap;

	unordered_map<ULONGLONG, st_USER*> _AccountUserMap;
};