#pragma once

class RoomNetServer;

class AuthRoom : IRoom
{
public:
	AuthRoom() {};

	virtual void EnqueueMessage(ULONGLONG sessionID, stRoomMessage* pMessage);

	// Enter, Leave 했을 때
	virtual void OnJoin(ULONGLONG sessionID, stRoomMessage* pMessage);
	virtual void OnLeave(ULONGLONG sessionID, stRoomMessage* pMessage);
	virtual void OnMessage(ULONGLONG sessionID, stRoomMessage* pMessage);
	virtual void OnUpdate();
	virtual void OnLateUpdate();
	virtual void OnSessionUpdate(ULONGLONG sessionID);
private:
	void AuthProc(ULONGLONG sessionID, RefCountPointer& cPacket);
	void EnterAuthRoom(ULONGLONG sessionID);
	void LeaveAuthRoom(ULONGLONG sessionID);

	// 중복 체크용 맵
	unordered_map<ULONGLONG, st_SESSION*> _SessionMap;
};