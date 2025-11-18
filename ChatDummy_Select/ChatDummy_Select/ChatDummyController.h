#pragma once

class ChatDummyController
{
public:
	ChatDummyController(DummyHandler* handler)
	{
		_dummyHandler = handler;
	}

	void InitController(int sessionCount, int startIdx, bool bTestTimeout);

	void InitTestDummy(int sessionCount, int startIdx);

	bool WorkByAction(ChatDummy* ptr);

	bool PacketProc(ChatDummy& dummy, RefCountPointer& cPacket);

	void HeartBeatProc(DWORD sessionID, DWORD nowTime);
	void Update();
	bool Skip();

	void OnConnect(DWORD sessionID);

	void OnDisconnect(DWORD SessionID);

	void OnRecv(DWORD SessionID, RefCountPointer& cpacket);

	//virtual void OnError(int errorcode, WCHAR* message) = 0;
protected:
	bool TimerErrorCheck(DWORD sessionID);

	DWORD _dwLoopCount;

	int _iSessionCount;
	int _iThreadCount;

	int _ilogicFrame = 0;

	SOCKADDR_IN _serverAddr;

	HANDLE _hLogUpdateEvent;
	HANDLE _htpsThreadHandle;
	unsigned int _tpsThreadID;

	ULONG _threadID = 1;
	ChatDummy _DummyArr[4000];
	DummyHandler* _dummyHandler;

	void PrintLog();
	void ResetTPS();

	// 스레드 함수들
	static unsigned int WINAPI LogingThread(LPVOID arg);
	static unsigned int WINAPI ChatDummyControlThread(LPVOID arg);
};