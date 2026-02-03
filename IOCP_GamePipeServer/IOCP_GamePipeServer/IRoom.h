#pragma once
#define dfROOM_AUTH 1001
#define dfROOM_ECHO 1011
#define dfFRAME 50
#include "ContentsDefine.h"
#include "LogManager.h"

class RoomNetServer;

class IRoom
{
public:
	bool DequeueMessage(stRoomMessage* pOutput);
	
	void EnqueueMessage(stRoomMessage* pMessage);

	// Init으로 해도 될듯
	void SetRoomInfo(DWORD roomNumber, RoomNetServer* pRoomNetServer);

	// Enter, Leave 했을 때
	virtual void OnJoin(ULONGLONG sessionID) = 0;
	virtual void OnLeave(ULONGLONG sessionID) = 0;
	virtual void OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket) = 0;
	virtual void OnUpdate() = 0;

	// 비정적 멤버는 인스턴스마다 다른 메모리를 가지는데, thread_local은
	// 인스턴스마다가 아니라, 스레드 마다 같은 메모리를 가지니 의미가 충돌한다.
	// 그래서 static으로 선언해야 한다.
	static thread_local stChatLog _pLog;
protected:
	st_USER* AllocUSER();
	void FreeUSER(st_USER* pUser);
	st_SESSION* AllocSESSION();
	void FreeSESSION(st_SESSION* pSession);

	LockFreeQueue<stRoomMessage*> _MessageQueue;
	RoomNetServer* _pRoomNetServer;

	DWORD _dwRoomNumber;
private:
	static unsigned int WINAPI RoomThread(LPVOID arg)
	{
		IRoom* thisPtr = (IRoom*)arg;
		LogController::GetInstance()->RegisterLogStruct(&_pLog);

		//stRoomMessage* pMessage = NULL;
		DWORD ret = 0;
		while (1)
		{
			thisPtr->OnUpdate();

			ret = WaitForSingleObject(thisPtr->_hQuitEvent, thisPtr->_dwFrameTime);
			if (ret == WAIT_OBJECT_0)
			{
				// 서버 종료
				return 0;
			}
		}
	}

	HANDLE _hQuitEvent;

	HANDLE _RoomThreadHandle;
	unsigned int _RoomThreadID;

	DWORD _dwFrameTime;
};