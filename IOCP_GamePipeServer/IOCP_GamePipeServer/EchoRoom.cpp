#include "Includes.h"
#include "ContentsDefine.h"
#include "CommonProtocol.h"
#include "NetServer_Pipe.h"
#include "RoomNetServer.h"
#include "IRoom.h"
#include "EchoRoom.h"
#include "ProcademyProfiler.h"

void EchoRoom::OnJoin(ULONGLONG sessionID)
{
	st_USER* pUser = NULL;
	if (!_pRoomNetServer->GetInfoFromSession(sessionID, (LPVOID*)&pUser))
	{
		return;
	}

	auto it = _UserMap.find(sessionID);
	if (it != _UserMap.end())
	{
		// 중복로그인 - sessionID
		_pRoomNetServer->Disconnect(sessionID);
		_UserMap.erase(sessionID);
		return;
	}

	it = _AccountUserMap.find(pUser->AccountNum);
	if (it != _AccountUserMap.end())
	{
		// 중복로그인 - sessionID
		_pRoomNetServer->Disconnect(sessionID);
		_AccountUserMap.erase(pUser->AccountNum);
		_UserMap.erase(sessionID);

		_pLog._dwGameUserCount--;
		_pLog._dwDuplicatedLoginTotal++;
	}

	_AccountUserMap.insert({ pUser->AccountNum, pUser });

	_pLog._dwGameUserCount++;
	_pLog._dwLoginMessageTPS++;

	// RES Send
	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	(*cPacket)->Initialize(sizeof(st_NetHeader));
	mpRESLogin(cPacket, true, pUser->AccountNum);
	_pRoomNetServer->SendPacket_UniCast(sessionID, cPacket);
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
		_pRoomNetServer->Disconnect(sessionID);
		// @@TODO : 로그 추가
		return;
	}

	(**cPacket) >> accountNum;
	(**cPacket) >> sendTick;

	auto it = _AccountUserMap.find(accountNum);
	if (it == _AccountUserMap.end())
	{
		_pRoomNetServer->Disconnect(sessionID);
		// @@TODO : 로그 추가
		return;
	}

	(*it).second->dwLastRecvTime = timeGetTime();

	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESEcho(cPacket, accountNum, sendTick);
	_pLog._dwEchoMessageTPS++;
	if (_pRoomNetServer->PostPacket(sessionID, cPacket))
		_pLog._dwSendMessageTPS++;
}

void EchoRoom::OnUpdate()
{
	//stRoomMessage Message;
	//while (!_MessageQueue.Empty())
	//{
	//	DequeueMessage(&Message);

	//	RoomMessageType type = Message.type;
	//	switch (type)
	//	{
	//	case ENTER:
	//		OnJoin(Message.sessionID);
	//		break;
	//	case LEAVE:
	//		OnLeave(Message.sessionID);
	//		break;
	//	case MESSAGE:
	//		OnMessage(Message.sessionID, Message.cPacket);
	//		break;
	//	}
	//}

	_pLog._dwGameFPS++;
}

void EchoRoom::OnSessionUpdate(ULONGLONG sessionID)
{

}

void EchoRoom::mpRESLogin(RefCountPointer& cPacket, BYTE status, ULONGLONG accountNum)
{
	WORD type = en_PACKET_CS_GAME_RES_LOGIN;

	(**cPacket) << type;
	(**cPacket) << status;
	(**cPacket) << accountNum;
}

void EchoRoom::mpRESEcho(RefCountPointer& cPacket, ULONGLONG accountNum, LONGLONG sendTick)
{
	WORD type = en_PACKET_CS_GAME_RES_ECHO;

	(**cPacket) << type;
	(**cPacket) << accountNum;
	(**cPacket) << sendTick;
}