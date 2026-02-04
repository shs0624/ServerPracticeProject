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
	virtual void OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket);
	virtual void OnUpdate();
	virtual void OnSessionUpdate(ULONGLONG sessionID);
private:
	void mpRESLogin(RefCountPointer& cPacket, BYTE status, ULONGLONG accountNum);
	void mpRESEcho(RefCountPointer& cPacket, ULONGLONG accountNum, LONGLONG sendTick);

	// 중복체크?
	unordered_map<ULONGLONG, st_USER*> _AccountUserMap;
};