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
		// @@TODO : 메세지가 도착했으니, 세션에 넣어주면 된다.
		EchoProc(sessionID, cPacket);
		//if (!cPacket.DecRefCount())
		//	_pLog._dwPacketPoolUse--;
	}

	virtual void inline OnUpdate()
	{
		// Enter, Leave 메세지 처리
		while (_MessageQueue->Size() > 0)
		{
			RefCountPointer cPacket;
			_MessageQueue->Dequeue(cPacket);

			ULONGLONG sessionID;
			WORD type;

			(**cPacket) >> sessionID;
			(**cPacket) >> type;

			switch (type)
			{
			case ENTER:
				EnterEchoRoom(sessionID);
				break;
			case LEAVE:
				LeaveEchoRoom(sessionID);
				break;
			}

			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;
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