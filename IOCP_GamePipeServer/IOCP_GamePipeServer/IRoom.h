#pragma once
#define dfROOM_AUTH 1001
#define dfROOM_ECHO 1011
#define dfFRAME 30
#include "ContentsDefine.h"
#include "LogManager.h"

class RoomNetServer;

class IRoom
{
public:
	DWORD GetRoomNumber() { return _dwRoomNumber; }
	
	// Enter, Leave 메세지가 처리 됐을 때 이벤트
	virtual void OnEnter(ULONGLONG sessionID) = 0;
	virtual void OnLeave(ULONGLONG sessionID) = 0;

	// 패킷이 도착했다는 메세지를 꺼낸 경우 호출하는 이벤트
	virtual void OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket) = 0;

	// 매 프레임마다 호출하는 함수. Session별로 업데이트가 필요해서 SessionUpdate도 추가
	virtual void OnUpdate() = 0;
	virtual void OnSessionUpdate(ULONGLONG sessionID) = 0;

	void EnqueueEntryMessage(ULONGLONG sessionID, WORD type);

	// Init으로 해도 될듯
	void SetRoomInfo(DWORD roomNumber, RoomNetServer* pRoomNetServer);

	void UpdateRoom()
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

			if (type == ENTER)
				EnterRoom(sessionID);
			else if (type == LEAVE)
				LeaveRoom(sessionID);

			cPacket.FreeRefPointer();
			_pRoomLog._dwPacketPoolUse--;
		}

		OnUpdate();
	}

	void RegisterLog();

	bool inline SleepCheck()
	{
		const ULONGLONG now = GetTickCount64();

		// 초과: 바로 다음 루프로 진행 (Sleep 없음)
		if (now >= _ulNextFrameTick)
		{
			// 여러 프레임 초과분을 한 번에 보정.
			ULONGLONG late = now - _ulNextFrameTick;
			ULONGLONG skip = late / _dwFrameTime + 1;
			_ulNextFrameTick += skip * _dwFrameTime;

			// 종료 이벤트만 즉시 확인
			return (WaitForSingleObject(_hQuitEvent, 0) != WAIT_OBJECT_0);
		}

		// 남은 시간: 그만큼만 대기
		DWORD waitMs = (_ulNextFrameTick - now);
		int ret = WaitForSingleObject(_hQuitEvent, waitMs);
		if (ret == WAIT_OBJECT_0)
			return false;

		_ulNextFrameTick += _dwFrameTime;
		return true;
	}

	// 비정적 멤버는 인스턴스마다 다른 메모리를 가지는데, thread_local은
	// 인스턴스마다가 아니라, 스레드 마다 같은 메모리를 가지니 의미가 충돌한다.
	// 그래서 static으로 선언해야 한다.
	static thread_local stChatLog _pRoomLog;
protected:
	st_USER* AllocUSER();
	void FreeUSER(st_USER* pUser);
	st_SESSION* AllocSESSION();
	void FreeSESSION(st_SESSION* pSession);

	// ENTER, LEAVE는 메세지 큐를 통해서 처리합니다.
	LockFreeQueue<RefCountPointer>* _MessageQueue;
	//std::stack<ULONGLONG> _SendIDStack;
	std::unordered_set<ULONGLONG> _SendIDSet;

	RoomNetServer* _pRoomNetServer;
	CNetServer* _pNetServer;

	ULONGLONG _ulNextFrameTick;
	DWORD _dwRoomNumber;
private:
	// ENTER 메세지가 처리될 때 호출할 함수
	void EnterRoom(ULONGLONG sessionID);
	// LEAVE 메세지가 처리될 때 호출할 함수
	void LeaveRoom(ULONGLONG sessionID);

	HANDLE _hQuitEvent;

	HANDLE _RoomThreadHandle;
	unsigned int _RoomThreadID;

	DWORD _dwFrameTime;
};