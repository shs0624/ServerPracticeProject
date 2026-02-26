#pragma once
#define dfFIXEDKEY 0x32
#define dfPROGRAM_KEY 0x77
#include "ContentsDefine.h"

class IRoom;

struct RoomInfo
{
	IRoom* pRoomPtr;
	vector<st_NetSession*> vNetSessionVec;
	DWORD dwRoomNumber;
};

class RoomNetServer : CNetServer
{
public:
	void InitRoomNetServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection);

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CNetServer::QuitServer();
	}

	bool RoomSendPacket(ULONGLONG sessionID, RefCountPointer& cPacket, bool pushHeader = true)
	{
		if (PostPacket(sessionID, cPacket, pushHeader))
		{
			_pLog._dwSendMessageTPS++;
			return true;
		}
		else
			return false;
	}

	bool GetPTRFromSession(ULONGLONG sessionID, LPVOID* ptr)
	{
		return GetInfoFromSession(sessionID, ptr);
	}

	bool SetPTRToSession(ULONGLONG sessionID, LPVOID ptr, DWORD roomNum)
	{
		return SetInfoToSession(sessionID, ptr, roomNum);
	}

	bool AddSessionToRoom(ULONGLONG sessionID, DWORD roomNumber);
	void RemoveSessionFromRoom(ULONGLONG sessionID, DWORD roomNumber);
	void MoveRoom(ULONGLONG sessionID, DWORD nowRoomNum, DWORD moveRoomNum);

	st_USER* AllocUSER() { return _UserPool->Alloc(); }
	void inline FreeUSER(st_USER* pUser) { _UserPool->Free(pUser); }
	st_SESSION* AllocSESSION() { return _SessionPool->Alloc(); }
	void inline FreeSESSION(st_SESSION* pSession) { _SessionPool->Free(pSession); }

	//virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr);
	virtual void OnRelease(ULONGLONG sessionID);
	virtual void OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message)
	{

	}
private:
	void InitRoom();

	static unsigned int WINAPI TimerThread(LPVOID arg)
	{

	}

	static unsigned int WINAPI RoomThread(LPVOID arg);

	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;

	HANDLE _TimerThreadHandle;
	unsigned int _TimerThreadID;

	HANDLE _AuthRoomThreadHandle;
	unsigned int _AuthRoomThreadID;

	HANDLE _EchoRoomThreadHandle;
	unsigned int _EchoRoomThreadID;

	procademy::CMemoryPool_LockFree<st_USER>* _UserPool;
	procademy::CMemoryPool_LockFree<st_SESSION>* _SessionPool;

	// AccountNum, 유저 구조체 - 중복 로그인 체크용
	/*unordered_map<ULONGLONG, st_USER*> _AccountNumUserMap;
	SRWLOCK _AccountNumUserMapLock;*/

	//// SessionID, 유저 구조체
	//unordered_map<ULONGLONG, st_USER*> _UserMap;
	//SRWLOCK _UserMapLock;

	//// SessionID, 세션 구조체
	//unordered_map<ULONGLONG, st_SESSION*> _SessionMap;
	//SRWLOCK _SessionMapLock;

	unordered_map<DWORD, RoomInfo*> _RoomMap;

	//static TLSMemoryPoolManager<stRoomMessage> _MessagePool;
};