#pragma once
#define dfFIXEDKEY 0x32
#define dfPROGRAM_KEY 0x77
#include "ContentsDefine.h"

class IRoom;

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

	void DisconnectSession(ULONGLONG sessionID)
	{
		Disconnect(sessionID);
	}

	bool GetPTRFromSession(ULONGLONG sessionID, LPVOID* ptr)
	{
		return GetInfoFromSession(sessionID, ptr);
	}

	bool SetPTRToSession(ULONGLONG sessionID, LPVOID ptr, DWORD roomNum)
	{
		return SetInfoToSession(sessionID, ptr, roomNum);
	}

	void MoveRoom(ULONGLONG sessionID, DWORD nowRoomNum, DWORD moveRoomNum);

	st_USER* AllocUSER() { return _UserPool->Alloc(); }
	void FreeUSER(st_USER* pUser) { _UserPool->Free(pUser); }
	st_SESSION* AllocSESSION() { return _SessionPool->Alloc(); }
	void FreeSESSION(st_SESSION* pSession) { _SessionPool->Free(pSession); }

	void FreeMessage(stRoomMessage* pMessage) { _MessagePool.Free(pMessage); }

	void mpRESLogin(RefCountPointer& cPacket, BYTE status, ULONGLONG accountNum);
	void mpRESEcho(RefCountPointer& cPacket, ULONGLONG accountNum, LONGLONG sendTick);

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

	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;

	HANDLE _TimerThreadHandle;
	unsigned int _TimerThreadID;

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

	unordered_map<DWORD, IRoom*> _RoomMap;

	static TLSMemoryPoolManager<stRoomMessage> _MessagePool;
};