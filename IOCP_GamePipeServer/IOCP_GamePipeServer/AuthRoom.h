#pragma once

class RoomNetServer;

class AuthRoom : IRoom
{
public:
	AuthRoom() {};

	// Enter, Leave 했을 때
	virtual void OnJoin(ULONGLONG sessionID);
	virtual void OnLeave(ULONGLONG sessionID);

	virtual void inline OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket)
	{
		// @@TODO : 메세지가 도착했으니, 세션에 넣어주면 된다.
		AuthProc(sessionID, cPacket);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
	}

	virtual void inline OnUpdate()
	{
		// Enter, Leave 메세지 처리
		while (!_MessageQueue->Empty())
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
				//OnJoin(pMessage->sessionID);
				EnterAuthRoom(sessionID);
				break;
			case LEAVE:
				//OnLeave(pMessage->sessionID);
				LeaveAuthRoom(sessionID);
				break;
			}

			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;
		}

		// @@TODO : 타이머 체크
		_pLog._dwAuthFPS++;
	}

	virtual void inline OnLateUpdate()
	{

	}

	virtual inline void OnSessionUpdate(ULONGLONG sessionID)
	{
		st_SESSION* pSession = NULL;
		auto it = _SessionMap.find(sessionID);
		if (it == _SessionMap.end())
		{
			return;
		}

		pSession = (*it).second;
	}
private:
	void AuthProc(ULONGLONG sessionID, RefCountPointer& cPacket);
	void EnterAuthRoom(ULONGLONG sessionID);
	void LeaveAuthRoom(ULONGLONG sessionID);

	// 중복 체크용 맵
	unordered_map<ULONGLONG, st_SESSION*> _SessionMap;
};