#pragma once

class RoomNetServer;

class AuthRoom : IRoom
{
public:
	AuthRoom() {}

	// Enter, Leave ÇßÀ» ¶§
	virtual void OnJoin(ULONGLONG sessionID);
	virtual void OnLeave(ULONGLONG sessionID);
	virtual void OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket);
	virtual void OnUpdate()
	{
		
	}
private:
	unordered_map<ULONGLONG, st_SESSION*> _SessionMap;
};