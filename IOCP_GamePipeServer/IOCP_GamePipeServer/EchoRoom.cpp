#include "Includes.h"
#include "ContentsDefine.h"
#include "CommonProtocol.h"
#include "NetServer_Pipe.h"
#include "RoomNetServer.h"
#include "IRoom.h"
#include "EchoRoom.h"
#include "ProcademyProfiler.h"

void EchoRoom::EnqueueMessage(ULONGLONG sessionID, stRoomMessage* pMessage)
{
	// 이미 설정된 유저가 있으니, 걔를 찾아오기
	_MessageQueue->Enqueue(pMessage);
}

void EchoRoom::OnJoin(ULONGLONG sessionID, stRoomMessage* pMessage)
{
	// Enter 메세지가 들어오는데, 이걸 내부에서 생성하는게 나을수도
	_MessageQueue->Enqueue(pMessage);
}

void EchoRoom::OnLeave(ULONGLONG sessionID, stRoomMessage* pMessage)
{
	// Leave 메세지가 들어오는데, 이걸 내부에서 생성하는게 나을수도
	_MessageQueue->Enqueue(pMessage);
}

void EchoRoom::OnMessage(ULONGLONG sessionID, stRoomMessage* pMessage)
{
	// @@TODO : 메세지가 도착했으니, 세션에 넣어주면 된다.
	RoomMessageType type = pMessage->type;
	switch (type)
	{
	case MESSAGE:
		EchoProc(pMessage->sessionID, pMessage->cPacket);
		break;
	}

	_pRoomNetServer->FreeMessage(pMessage);
}

void EchoRoom::OnUpdate()
{
	// Enter, Leave 메세지 처리
	while (_MessageQueue->Size() > 0)
	{
		stRoomMessage* pMessage;
		_MessageQueue->Dequeue(pMessage);
		if (pMessage == NULL)
			break; // Disconnect?

		RoomMessageType type = pMessage->type;
		switch (type)
		{
		case ENTER:
			EnterEchoRoom(pMessage->sessionID);
			break;
		case LEAVE:
			LeaveEchoRoom(pMessage->sessionID);
			break;
		}

		_pRoomNetServer->FreeMessage(pMessage);
	}

	_pLog._dwGameFPS++;
}

void EchoRoom::OnLateUpdate()
{
	while (!_SendIDStack.empty())
	{
		ULONGLONG sessionID = _SendIDStack.top();
		_SendIDStack.pop();

		_pNetServer->PostSend(sessionID);
	}
}

void EchoRoom::OnSessionUpdate(ULONGLONG sessionID)
{
	st_USER* pUser = NULL;
	auto it = _UserMap.find(sessionID);
	if (it == _UserMap.end())
	{
		return;
	}

	pUser = (*it).second;
	// 메세지 있는지 체크
	//while (!pUser->_MessageQ->Empty())
	//{
	//	stRoomMessage* pMessage;
	//	pUser->_MessageQ->Dequeue(pMessage);
	//	if (pMessage == NULL)
	//		break; // Disconnect?

	//	RoomMessageType type = pMessage->type;
	//	switch (type)
	//	{
	//	case MESSAGE:
	//		EchoProc(pMessage->sessionID, pMessage->cPacket);
	//		break;
	//	}

	//	_pRoomNetServer->FreeMessage(pMessage);
	//}
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

void EchoRoom::EchoProc(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	WORD type;
	ULONGLONG accountNum;
	LONGLONG sendTick;

	(**cPacket) >> type;
	if (type != en_PACKET_CS_GAME_REQ_ECHO)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		_pNetServer->Disconnect(sessionID);
		// @@TODO : 로그 추가
		return;
	}

	(**cPacket) >> accountNum;
	(**cPacket) >> sendTick;

	// @@TODO : 타이머는 라이브러리에서 하기.

	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESEcho(cPacket, accountNum, sendTick);
	_pLog._dwEchoMessageTPS++;
	//if (_pNetServer->PostPacket(sessionID, cPacket))
	//	_pLog._dwSendMessageTPS++;
	if (_pNetServer->EnqueueSendBuffer(sessionID, cPacket))
		_pLog._dwSendMessageTPS++;
	//if(_pNetServer->SendPacket_UniCast(sessionID, cPacket))
	//	_pLog._dwSendMessageTPS++;

	_SendIDStack.push(sessionID);
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
		_pNetServer->Disconnect(sessionID);
		_AccountUserMap.erase(pUser->AccountNum);
		_UserMap.erase(sessionID);

		_pLog._dwGameUserCount--;
		_pLog._dwDuplicatedLoginTotal++;
	}

	_UserMap.insert({ sessionID, pUser });
	_AccountUserMap.insert({ pUser->AccountNum, pUser });

	_pRoomNetServer->AddSessionToRoom(sessionID, _dwRoomNumber);

	_pLog._dwGameUserCount++;
	_pLog._dwLoginMessageTPS++;

	// RES Send
	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
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
		_pLog._dwGameUserCount--;

		_pRoomNetServer->RemoveSessionFromRoom(sessionID, _dwRoomNumber);
	}
}
