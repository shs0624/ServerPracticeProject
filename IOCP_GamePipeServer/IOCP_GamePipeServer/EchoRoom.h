#pragma once
#include "ContentsDefine.h"
#include "CommonProtocol.h"

class RoomNetServer;

class EchoRoom : IRoom
{
public:
	EchoRoom() {}

	virtual void EnqueueMessage(ULONGLONG sessionID, stRoomMessage* pMessage);

	// Enter, Leave 했을 때
	virtual void OnJoin(ULONGLONG sessionID, stRoomMessage* pMessage);
	virtual void OnLeave(ULONGLONG sessionID, stRoomMessage* pMessage);
	virtual void OnMessage(ULONGLONG sessionID, stRoomMessage* pMessage);
	virtual void OnUpdate();
	virtual void OnLateUpdate();
	virtual void OnSessionUpdate(ULONGLONG sessionID);
private:
	void EchoProc(ULONGLONG sessionID, RefCountPointer& cPacket);
	void EnterEchoRoom(ULONGLONG sessionID);
	void LeaveEchoRoom(ULONGLONG sessionID);

	void mpRESLogin(RefCountPointer& cPacket, BYTE status, ULONGLONG accountNum);
	void mpRESEcho(RefCountPointer& cPacket, ULONGLONG accountNum, LONGLONG sendTick);

	unordered_map<ULONGLONG, st_USER*> _UserMap;
	// 중복체크?
	unordered_map<ULONGLONG, st_USER*> _AccountUserMap;
};