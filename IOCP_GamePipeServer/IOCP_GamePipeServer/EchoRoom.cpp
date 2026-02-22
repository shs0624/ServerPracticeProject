#include "Includes.h"
#include "ContentsDefine.h"
#include "CommonProtocol.h"
#include "NetServer_Pipe.h"
#include "RoomNetServer.h"
#include "IRoom.h"
#include "EchoRoom.h"
#include "ProcademyProfiler.h"

void EchoRoom::OnEnter(ULONGLONG sessionID)
{
	EnterEchoRoom(sessionID);
}

void EchoRoom::OnLeave(ULONGLONG sessionID)
{
	LeaveEchoRoom(sessionID);
}

void EchoRoom::mpRESLogin(RefCountPointer& cPacket, BYTE status, ULONGLONG accountNum)
{
	WORD type = en_PACKET_CS_GAME_RES_LOGIN;

	(**cPacket) << type;
	(**cPacket) << status;
	(**cPacket) << accountNum;
}

void EchoRoom::EchoProc(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	//WORD type;
	//ULONGLONG accountNum;
	//LONGLONG sendTick;

	//(**cPacket) >> type;
	//if (type != en_PACKET_CS_GAME_REQ_ECHO)
	//{
	//	if (!cPacket.DecRefCount())
	//		_pLog._dwPacketPoolUse--;
	//	_pNetServer->Disconnect(sessionID);
	//	// @@TODO : 로그 추가
	//	return;
	//}

	//(**cPacket) >> accountNum;
	//(**cPacket) >> sendTick;

	//// @@TODO : 타이머는 라이브러리에서 하기.

	//(*cPacket)->Clear(sizeof(st_NetHeader));
	//mpRESEcho(cPacket, accountNum, sendTick);
	//_pLog._dwEchoMessageTPS++;
	//if (_pNetServer->EnqueueSendBuffer(sessionID, cPacket))
	//	_pLog._dwSendMessageTPS++;


	//if (_pNetServer->SendPacket_UniCast(sessionID, cPacket))
	//	_pLog._dwSendMessageTPS++;
	//if (_pNetServer->PostPacket(sessionID, cPacket))
	//	_pLog._dwSendMessageTPS++;

	//_SendIDSet.insert(sessionID);
}

void EchoRoom::EnterEchoRoom(ULONGLONG sessionID)
{
	auto it = _UserMap.find(sessionID);
	if (it != _UserMap.end())
	{
		return;
	}

	st_USER* pUser = NULL;
	if (!_pRoomNetServer->GetPTRFromSession(sessionID, (LPVOID*)&pUser))
	{
		return;
	}

	it = _AccountUserMap.find(pUser->AccountNum);
	if (it != _AccountUserMap.end())
	{
		// 중복로그인 - sessionID
		ULONGLONG disconnectID = (*it).second->ulSessionID;
		_pNetServer->Disconnect(disconnectID);
		_AccountUserMap.erase(pUser->AccountNum);
		_UserMap.erase(disconnectID);

		_pRoomLog._dwGameUserCount--;
		_pRoomLog._dwDuplicatedLoginTotal++;
	}

	_UserMap.insert({ sessionID, pUser });
	_AccountUserMap.insert({ pUser->AccountNum, pUser });

	_pRoomLog._dwGameUserCount++;
	_pRoomLog._dwLoginMessageTPS++;

	// RES Send
	//RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	RefCountPointer cPacket = RefCountPointer::MakePtr();
	_pRoomLog._dwPacketPoolUse++;

	(*cPacket)->Initialize(sizeof(st_NetHeader));
	mpRESLogin(cPacket, true, pUser->AccountNum);
	_pNetServer->SendPacket_UniCast(sessionID, cPacket);
}

void EchoRoom::LeaveEchoRoom(ULONGLONG sessionID)
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
		_pRoomLog._dwGameUserCount--;
	}
}
