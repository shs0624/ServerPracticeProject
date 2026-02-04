#pragma once

class RoomNetServer;

class AuthRoom : IRoom
{
public:
	AuthRoom() {}

	// Enter, Leave 했을 때
	virtual void OnJoin(ULONGLONG sessionID);
	virtual void OnLeave(ULONGLONG sessionID);
	virtual void OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket);
	virtual void OnUpdate();
	virtual void OnSessionUpdate(ULONGLONG sessionID);
private:
	// 중복 체크용 맵
	unordered_map<ULONGLONG, st_SESSION*> _SessionMap;
	// 순회용 벡터?
	vector<st_SESSION*> _SessionVec;
};