#include "Includes.h"
#include "LanServer.h"
#include "NetServer.h"
#include "LogManager.h"
#include "MonitorProtocol.h"
#include "MonitorClientServer.h"
#include "MonitorDataManager.h"
#include "MonitorChatServer.h"

void MonitorChatServer::InitMonitorChatServer(MonitorDataManager* pManager, ULONG ip, int maxConnection, unsigned char programKey, unsigned char fixedKey)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	int workCount = (int)si.dwNumberOfProcessors * 2;
	StartLanServer(ip, dfSERVERPORT_CHAT, workCount / 2, true, maxConnection, programKey, fixedKey);

	_pMonitorDataManager = pManager;

	InitializeSRWLock(&_UserMapLock);
	InitializeSRWLock(&_SessionMapLock);

	_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	_hTimeoutEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

	_UserPool = new procademy::CMemoryPool_LockFree<st_ChatUSER>(maxConnection, false, false);
	_SessionPool = new procademy::CMemoryPool_LockFree<st_ChatSESSION>(maxConnection, false, false);
}

bool MonitorChatServer::OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr)
{
	st_ChatSESSION* pSession = _SessionPool->Alloc();

	pSession->ClientAddr = clientAddr;
	pSession->ulSessionID = sessionID;
	pSession->dwLastRecvTime = timeGetTime();

	AcquireSRWLockExclusive(&_SessionMapLock);
	_SessionMap.insert({ sessionID, pSession });
	ReleaseSRWLockExclusive(&_SessionMapLock);

	_pLog._dwChatAcceptTotal++;

	return true;
}

void MonitorChatServer::OnRelease(ULONGLONG sessionID)
{
	AcquireSRWLockExclusive(&_UserMapLock);
	auto itUser = _UserMap.find(sessionID);
	if (itUser != _UserMap.end())
	{
		st_ChatUSER* pUser = (*itUser).second;
		_UserMap.erase(sessionID);
		ReleaseSRWLockExclusive(&_UserMapLock);

		_UserPool->Free(pUser);
	}
	else
		ReleaseSRWLockExclusive(&_UserMapLock);

	AcquireSRWLockExclusive(&_SessionMapLock);
	auto itSession = _SessionMap.find(sessionID);
	if (itSession != _SessionMap.end())
	{
		st_ChatSESSION* pSession = (*itSession).second;
		_SessionMap.erase(sessionID);
		_SessionPool->Free(pSession);

		ReleaseSRWLockExclusive(&_SessionMapLock);
	}
	else
		ReleaseSRWLockExclusive(&_SessionMapLock);

	_pLog._dwServerCount--;
}

// 채팅서버에서 받을 메세지는 로그인 뿐
void MonitorChatServer::OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket)
{
	_pLog._dwChatRecvMessageTPS++;

	// 타입 꺼낸 후 그 데이터를 Manager에 전달
	WORD type;
	(**cpacket) >> type;

	switch ((en_PACKET_TYPE)type)
	{
	case en_PACKET_SS_MONITOR_DATA_UPDATE:
		MessageProc_UpdateMonitorData(cpacket, sessionID);
		break;
	case en_PACKET_SS_MONITOR_LOGIN:
		MessageProc_ServerMonitorLogin(cpacket, sessionID);
		break;
	}
	
}

void MonitorChatServer::OnError(int errorcode, WCHAR* message)
{

}

void MonitorChatServer::MessageProc_ServerMonitorLogin(RefCountPointer& cPacket, ULONGLONG sessionID)
{
	// serverNum - int
	int serverNum;
	(**cPacket) >> serverNum;

	// 로그인..
	AcquireSRWLockExclusive(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	if (it != _UserMap.end())
	{
		// 중복 로그인 처리 기존 것 삭제
		Disconnect((*it).second->ulSessionID);

		_pLog._dwDuplicatedLoginTotal++;
	}
	ReleaseSRWLockExclusive(&_UserMapLock);

	st_ChatUSER* userPtr = _UserPool->Alloc();

	userPtr->ulSessionID = sessionID;
	userPtr->dwLastRecvTime = timeGetTime();
	userPtr->iServerNum = serverNum;

	AcquireSRWLockExclusive(&_UserMapLock);
	_UserMap.insert({ userPtr->ulSessionID, userPtr });
	ReleaseSRWLockExclusive(&_UserMapLock);

	AcquireSRWLockExclusive(&_SessionMapLock);
	auto itSession = _SessionMap.find(sessionID);
	if (itSession != _SessionMap.end())
	{
		st_ChatSESSION* ptr = (*itSession).second;

		userPtr->ClientAddr = ptr->ClientAddr;
		_SessionMap.erase(sessionID);
		_SessionPool->Free(ptr);
	}
	ReleaseSRWLockExclusive(&_SessionMapLock);

	_pLog._dwServerCount++;
}

// 채팅서버에서 데이터를 받았으니, 데이터를 저장하고 클라에 전송
void MonitorChatServer::MessageProc_UpdateMonitorData(RefCountPointer& cPacket, ULONGLONG sessionID)
{
	// Type(BYTE), Value(int) ,Time(int)
	AcquireSRWLockExclusive(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	if (it == _UserMap.end())
	{
		return;
	}
	else
		ReleaseSRWLockExclusive(&_UserMapLock);

	BYTE type;
	(**cPacket) >> type;

	int value;
	(**cPacket) >> value;

	int time;
	(**cPacket) >> time;

	//받은 데이터를 갱신하고, 그 데이터만 전송
	_pMonitorDataManager->UpdateData((*it).second->iServerNum, type, value, time);
	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;
}

