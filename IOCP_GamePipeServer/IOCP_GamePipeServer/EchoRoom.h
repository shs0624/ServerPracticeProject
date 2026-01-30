#pragma once
#include "ContentsDefine.h"
#include "CommonProtocol.h"

class RoomNetServer;

class EchoRoom : IRoom
{
public:
	EchoRoom() {}

	// Enter, Leave ÇßÀ» ¶§
	virtual void OnJoin(ULONGLONG sessionID);
	virtual void OnLeave(ULONGLONG sessionID);
	virtual void OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket);
	virtual void OnUpdate()
	{

	}
private:
	unordered_map<ULONGLONG, st_USER*> _UserMap;
	unordered_map<ULONGLONG, st_USER*> _AccountUserMap;
};