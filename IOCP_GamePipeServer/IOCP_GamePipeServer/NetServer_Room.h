#pragma once
#define dfROOM_AUTH 1001
#define dfROOM_ECHO 1011
#define dfFRAME 30
#include "Structs.h"
#include "RoomNetServer.h"
#include "LogManager.h"
#include "IRoomFactory.h"

// 빌드에러 방지를 위한 정의
thread_local stChatLog IRoom::_pLog;

class IRoom
{
public:
	bool DequeueMessage(stRoomMessage* pOutput)
	{
		stRoomMessage Message;
		stRoomMessage* ptr = &Message;
		if (!_MessageQueue.Empty())
		{
			_MessageQueue.Dequeue(ptr);

			pOutput->cPacket = ptr->cPacket;
			pOutput->sessionID = ptr->sessionID;
			pOutput->type = ptr->type;

			_pRoomNetServer->FreeMessage(ptr);
			return true;
		}
		else
			return false;
	}

	virtual void EnqueueMessage(stRoomMessage* pMessage)
	{
		_MessageQueue.Enqueue(pMessage);

		SetEvent(_hThreadEvent);
	}

	// Init으로 해도 될듯
	virtual void SetRoomInfo(DWORD roomNumber, RoomNetServer* pRoomNetServer)
	{
		_dwFrameTime = 1000 / dfFRAME;
		_dwRoomNumber = roomNumber;
		_pRoomNetServer = pRoomNetServer;
	}

	

	// Enter, Leave 했을 때
	virtual void OnJoin(ULONGLONG sessionID) = 0;
	virtual void OnLeave(ULONGLONG sessionID) = 0;
	virtual void OnUpdate() = 0;
	virtual void OnMessage(ULONGLONG sessionID, RefCountPointer& cPacket) = 0;

	
protected:
	st_USER* AllocUSER() { return _pRoomNetServer->AllocUSER(); }
	void FreeUSER(st_USER* pUser) { _pRoomNetServer->FreeUSER(pUser); }
	st_SESSION* AllocSESSION() { return _pRoomNetServer->AllocSESSION(); }
	void FreeSESSION(st_SESSION* pSession) { _pRoomNetServer->FreeSESSION(pSession); }

	// 비정적 멤버는 인스턴스마다 다른 메모리를 가지는데, thread_local은
	// 인스턴스마다가 아니라, 스레드 마다 같은 메모리를 가지니 의미가 충돌한다.
	// 그래서 static으로 선언해야 한다.
	static thread_local stChatLog _pLog;

	LockFreeQueue<stRoomMessage*> _MessageQueue;
	RoomNetServer* _pRoomNetServer;

	DWORD _dwRoomNumber;
private:
	static unsigned int WINAPI RoomThread(LPVOID arg)
	{
		IRoom* thisPtr = (IRoom*)arg;
		LogController::GetInstance()->RegisterLogStruct(&_pLog);

		stRoomMessage* pMessage = NULL;
		DWORD ret = 0;
		while (1)
		{
			while (!thisPtr->_MessageQueue.Empty())
			{
				thisPtr->_MessageQueue.Dequeue(pMessage);

				RoomMessageType type = pMessage->type;
				switch (type)
				{
				case ENTER:
					thisPtr->OnJoin(pMessage->sessionID);
					break;
				case LEAVE:
					thisPtr->OnLeave(pMessage->sessionID);
					break;
				case MESSAGE:
					thisPtr->OnMessage(pMessage->sessionID, pMessage->cPacket);
					break;
				}
			}

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
	HANDLE _hThreadEvent;

	DWORD _dwFrameTime;
};