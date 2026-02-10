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
	bool FreeMessage(stRoomMessage* pOutput);
	
	// Init으로 해도 될듯
	void SetRoomInfo(DWORD roomNumber, RoomNetServer* pRoomNetServer);

	// ENTER, LEAVE 메세지를 넣는 함수
	virtual void EnqueueMessage(ULONGLONG sessionID, stRoomMessage* pMessage) = 0;

	void UpdateSession();

	void RegisterLog();

	bool SleepCheck();

	// 지금은 Enter, Leave 메세지가 처리 됐을 때, 즉 OnMessage가 호출하는 구조라 옳지않음.
	virtual void OnJoin(ULONGLONG sessionID, stRoomMessage* pMessage) = 0;
	virtual void OnLeave(ULONGLONG sessionID, stRoomMessage* pMessage) = 0;
	virtual void OnMessage(ULONGLONG sessionID, stRoomMessage* pMessage) = 0;
	virtual void OnUpdate() = 0;
	virtual void OnSessionUpdate(ULONGLONG sessionID) = 0;

	// 비정적 멤버는 인스턴스마다 다른 메모리를 가지는데, thread_local은
	// 인스턴스마다가 아니라, 스레드 마다 같은 메모리를 가지니 의미가 충돌한다.
	// 그래서 static으로 선언해야 한다.
	static thread_local stChatLog _pLog;
protected:
	st_USER* AllocUSER();
	void FreeUSER(st_USER* pUser);
	st_SESSION* AllocSESSION();
	void FreeSESSION(st_SESSION* pSession);

	// ENTER, LEAVE는 메세지 큐를 통해서 처리합니다.
	LockFreeQueue<stRoomMessage*>* _MessageQueue;

	RoomNetServer* _pRoomNetServer;
	CNetServer* _pNetServer;

	DWORD _dwRoomNumber;
private:
	//static unsigned int WINAPI RoomThread(LPVOID arg)
	//{
	//	IRoom* thisPtr = (IRoom*)arg;
	//	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	//	DWORD ret = 0;
	//	while (1)
	//	{
	//		// 세션별로 해야하는 작업 순회시키기
	//		for (int i = 0; i < thisPtr->_SessionVec.size(); i++)
	//		{
	//			st_NetSession* pSession = thisPtr->_SessionVec[i];
	//			
	//			thisPtr->OnSessionUpdate(pSession->ulSessionID);
	//		}

	//		thisPtr->OnUpdate();

	//		// Leave체크?

	//		ret = WaitForSingleObject(thisPtr->_hQuitEvent, thisPtr->_dwFrameTime);
	//		if (ret == WAIT_OBJECT_0)
	//		{
	//			// 서버 종료
	//			return 0;
	//		}
	//	}
	//}

	HANDLE _hQuitEvent;

	HANDLE _RoomThreadHandle;
	unsigned int _RoomThreadID;

	DWORD _dwFrameTime;
};