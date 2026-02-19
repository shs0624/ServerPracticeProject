#pragma once
#include "ContentsDefine.h"
#include "CommonProtocol.h"

class RoomNetServer;

class EchoRoom : IRoom
{
public:
	EchoRoom() {}

	// Enter, Leave 했을 때
	virtual void OnJoin(ULONGLONG sessionID);
	virtual void OnLeave(ULONGLONG sessionID);
	virtual void inline OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket)
	{
		//Profiler("OnMessage-Echo");
		WORD type;
		ULONGLONG accountNum;
		LONGLONG sendTick;

		(**cPacket) >> type;
		if (type != en_PACKET_CS_GAME_REQ_ECHO)
		{
			_pNetServer->Disconnect(sessionID);

			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;
			//cPacket.FreeRefPointer();
			//_pLog._dwPacketPoolUse--;

			// @@TODO : 로그 추가
			return;
		}

		(**cPacket) >> accountNum;
		(**cPacket) >> sendTick;

		// @@TODO : 타이머는 라이브러리에서 하기.

		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESEcho(cPacket, accountNum, sendTick);
		_pLog._dwEchoMessageTPS++;
		if (_pNetServer->EnqueueSendBuffer(sessionID, cPacket))
			_pLog._dwSendMessageTPS++;
	}

	virtual void inline OnUpdate()
	{
		// Enter, Leave 메세지 처리
		while (_MessageQueue->Size() > 0)
		{
			RefCountPointer cPacket;
			_MessageQueue->Dequeue_NoLockFree(cPacket);

			ULONGLONG sessionID;
			WORD type;

			(**cPacket) >> sessionID;
			(**cPacket) >> type;

			if (type == ENTER)
				EnterEchoRoom(sessionID);
			else if (type == LEAVE)
				LeaveEchoRoom(sessionID);

			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;
			//cPacket.FreeRefPointer();
			//_pLog._dwPacketPoolUse--;
		}

		_pLog._dwGameFPS++;
	}

	virtual void inline OnLateUpdate()
	{
		//for (auto it = _SendIDSet.begin(); it != _SendIDSet.end(); it++)
		//{
		//	ULONGLONG sessionID = (*it);

		//	_pNetServer->PostSend(sessionID);
		//}

		//_SendIDSet.clear();
	}

	virtual void inline OnSessionUpdate(ULONGLONG sessionID)
	{
		// 이 전에 뭔가 해야할것같은데
		_pNetServer->PostSend(sessionID);
	}
private:
	void EchoProc(ULONGLONG sessionID, RefCountPointer& cPacket);
	void EnterEchoRoom(ULONGLONG sessionID);
	void LeaveEchoRoom(ULONGLONG sessionID);

	void mpRESLogin(RefCountPointer& cPacket, BYTE status, ULONGLONG accountNum);
	void inline mpRESEcho(RefCountPointer& cPacket, ULONGLONG accountNum, LONGLONG sendTick)
	{
		WORD type = en_PACKET_CS_GAME_RES_ECHO;

		(**cPacket) << type;
		(**cPacket) << accountNum;
		(**cPacket) << sendTick;
	}

	unordered_map<ULONGLONG, st_USER*> _UserMap;
	// 중복체크?
	unordered_map<ULONGLONG, st_USER*> _AccountUserMap;
};