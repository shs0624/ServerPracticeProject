#include "Includes.h"
#include "ContentsDefine.h"
#include "CommonProtocol.h"
#include "NetServer_Pipe.h"
#include "RoomNetServer.h"
#include "IRoom.h"
#include "AuthRoom.h"

void AuthRoom::EnqueueMessage(ULONGLONG sessionID, stRoomMessage* pMessage)
{
	// @@TODO : ENTER 메세지만 넣자.
	_MessageQueue->Enqueue(pMessage);
}

void AuthRoom::OnJoin(ULONGLONG sessionID, stRoomMessage* pMessage)
{
	// Enter 메세지가 들어오는데, 이걸 내부에서 생성하는게 나을수도
	_MessageQueue->Enqueue(pMessage);
}

void AuthRoom::OnLeave(ULONGLONG sessionID, stRoomMessage* pMessage)
{
	// Leave 메세지가 들어오는데, 이걸 내부에서 생성하는게 나을수도
	_MessageQueue->Enqueue(pMessage);
}

void AuthRoom::OnMessage(ULONGLONG sessionID, stRoomMessage* pMessage)
{
	// @@TODO : 메세지가 도착했으니, 세션에 넣어주면 된다.
	RoomMessageType type = pMessage->type;
	switch (type)
	{
	case MESSAGE:
		AuthProc(sessionID, pMessage->cPacket);
		break;
	}

	_pRoomNetServer->FreeMessage(pMessage);
}

void AuthRoom::OnUpdate()
{
	// Enter, Leave 메세지 처리
	while (!_MessageQueue->Empty())
	{
		stRoomMessage* pMessage;
		_MessageQueue->Dequeue(pMessage);
		if (pMessage == NULL)
			break; // Disconnect?

		RoomMessageType type = pMessage->type;
		switch (type)
		{
		case ENTER:
			//OnJoin(pMessage->sessionID);
			EnterAuthRoom(pMessage->sessionID);
			break;
		case LEAVE:
			//OnLeave(pMessage->sessionID);
			LeaveAuthRoom(pMessage->sessionID);
			break;
		}

		_pRoomNetServer->FreeMessage(pMessage);
	}

	// @@TODO : 타이머 체크

	_pLog._dwAuthFPS++;
}

void AuthRoom::OnLateUpdate()
{
	//while (!_SendIDStack.empty())
	//{
	//	ULONGLONG sessionID = _SendIDStack.top();
	//	_SendIDStack.pop();

	//	_pNetServer->PostSend(sessionID);
	//}
}

void AuthRoom::OnSessionUpdate(ULONGLONG sessionID)
{
	st_SESSION* pSession = NULL;
	auto it = _SessionMap.find(sessionID);
	if (it == _SessionMap.end())
	{
		return;
	}

	pSession = (*it).second;
	// 메세지 있는지 체크
	//while (!pSession->_MessageQ->Empty())
	//{
	//	stRoomMessage* pMessage;
	//	pSession->_MessageQ->Dequeue(pMessage);
	//	if (pMessage == NULL)
	//		break; // Disconnect?

	//	RoomMessageType type = pMessage->type;
	//	switch (type)
	//	{
	//	case MESSAGE:
	//		AuthProc(sessionID, pMessage->cPacket);
	//		break;
	//	}

	//	_pRoomNetServer->FreeMessage(pMessage);
	//}
}

void AuthRoom::AuthProc(ULONGLONG sessionID, RefCountPointer& cPacket)
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

	_pRoomNetServer->AddSessionToRoom(sessionID, _dwRoomNumber);

	_pLog._dwAuthUserCount++;
}

void AuthRoom::LeaveAuthRoom(ULONGLONG sessionID)
{
	st_SESSION* pSession = NULL;
	auto it = _SessionMap.find(sessionID);
	if (it == _SessionMap.end())
		return;

	pSession = (*it).second;

	_pLog._dwAuthUserCount--;
	_SessionMap.erase(sessionID);

	if (pSession != NULL)
		FreeSESSION(pSession);

	_pRoomNetServer->RemoveSessionFromRoom(sessionID, _dwRoomNumber);
}
