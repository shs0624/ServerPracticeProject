#include "Includes.h"
#include "ContentsDefine.h"
#include "CommonProtocol.h"
#include "NetServer_Pipe.h"
#include "RoomNetServer.h"
#include "IRoom.h"
#include "EchoRoom.h"

void EchoRoom::OnJoin(ULONGLONG sessionID)
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
		return;
	}

	it = _AccountUserMap.find(pUser->AccountNum);
	if (it != _AccountUserMap.end())
	{
		// 중복로그인 - sessionID
		_pRoomNetServer->DisconnectSession(sessionID);
		_AccountUserMap.erase(pUser->AccountNum);
		_UserMap.erase(sessionID);

		_pLog._dwGameUserCount--;
		_pLog._dwDuplicatedLoginTotal++;
	}

	_UserMap.insert({ sessionID, pUser });
	_AccountUserMap.insert({ pUser->AccountNum, pUser });

	_pLog._dwGameUserCount++;
	_pLog._dwLoginMessageTPS++;

	// RES Send
	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	(*cPacket)->Initialize(sizeof(st_NetHeader));
	_pRoomNetServer->mpRESLogin(cPacket, true, pUser->AccountNum);
	_pRoomNetServer->RoomSendPacket(sessionID, cPacket);
}

void EchoRoom::OnLeave(ULONGLONG sessionID)
{
	auto it = _UserMap.find(sessionID);
	if (it != _UserMap.end())
	{
		st_USER* pUser = (*it).second;
		_UserMap.erase(sessionID);

		it = _AccountUserMap.find(pUser->AccountNum);
		if (it != _AccountUserMap.end())
		{
			_AccountUserMap.erase(pUser->AccountNum);
		}

		FreeUSER(pUser);
		_pLog._dwGameUserCount--;
	}
}

void EchoRoom::OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	WORD type;
	ULONGLONG accountNum;
	LONGLONG sendTick;

	(**cPacket) >> type;
	if (type != en_PACKET_CS_GAME_REQ_ECHO)
	{
		_pRoomNetServer->DisconnectSession(sessionID);
		// @@TODO : 로그 추가
		return;
	}

	(**cPacket) >> accountNum;
	(**cPacket) >> sendTick;

	auto it = _AccountUserMap.find(accountNum);
	if (it == _AccountUserMap.end())
	{
		_pRoomNetServer->DisconnectSession(sessionID);
		// @@TODO : 로그 추가
		return;
	}

	(*it).second->dwLastRecvTime = timeGetTime();

	(*cPacket)->Clear(sizeof(st_NetHeader));
	_pRoomNetServer->mpRESEcho(cPacket, accountNum, sendTick);
	_pLog._dwEchoMessageTPS++;
	if (_pRoomNetServer->RoomSendPacket(sessionID, cPacket))
		_pLog._dwSendMessageTPS++;
}

void EchoRoom::OnUpdate()
{
	stRoomMessage* pMessage = NULL;
	while (!_MessageQueue.Empty())
	{
		_MessageQueue.Dequeue(pMessage);

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

	_pLog._dwGameFPS++;
}