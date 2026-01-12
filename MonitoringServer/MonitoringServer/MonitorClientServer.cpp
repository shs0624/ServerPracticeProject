#include "Includes.h"
#include "LanServer.h"
#include "MonitorProtocol.h"
#include "MonitorClientServer.h"
#include "MonitorDataManager.h"
#include "MonitorChatServer.h"


void MonitorClientServer::InitMonitorClientServer(ULONG ip, int maxConnection, unsigned char programKey, unsigned char fixedKey)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	int workCount = (int)si.dwNumberOfProcessors * 2;
	StartLanServer(ip, dfSERVERPORT_CLIENT, workCount / 2, true, maxConnection, programKey, fixedKey);

	InitializeSRWLock(&_UserMapLock);
	InitializeSRWLock(&_SessionMapLock);

	_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	_hTimeoutEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

	_UserPool = new procademy::CMemoryPool_LockFree<st_ClientUSER>(maxConnection, false, false);
	_SessionPool = new procademy::CMemoryPool_LockFree<st_ClientSESSION>(maxConnection, false, false);
}

bool MonitorClientServer::OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr)
{
	st_ClientSESSION* pSession = _SessionPool->Alloc();

	pSession->ClientAddr = clientAddr;
	pSession->ulSessionID = sessionID;
	pSession->dwLastRecvTime = timeGetTime();

	AcquireSRWLockExclusive(&_SessionMapLock);
	_SessionMap.insert({ sessionID, pSession });
	ReleaseSRWLockExclusive(&_SessionMapLock);

	_pLog._dwClientAcceptTotal++;

	return true;
}

void MonitorClientServer::OnRelease(ULONGLONG sessionID)
{
	AcquireSRWLockExclusive(&_UserMapLock);
	auto itUser = _UserMap.find(sessionID);
	if (itUser != _UserMap.end())
	{
		st_ClientUSER* pUser = (*itUser).second;
		_UserMap.erase(sessionID);
		ReleaseSRWLockExclusive(&_UserMapLock);

		_pLog._dwDuplicatedLoginTotal++;

		_UserPool->Free(pUser);
	}
	else
		ReleaseSRWLockExclusive(&_UserMapLock);

	AcquireSRWLockExclusive(&_SessionMapLock);
	auto itSession = _SessionMap.find(sessionID);
	if (itSession != _SessionMap.end())
	{
		st_ClientSESSION* pSession = (*itSession).second;
		_SessionMap.erase(sessionID);
		_SessionPool->Free(pSession);

		ReleaseSRWLockExclusive(&_SessionMapLock);
	}
	else
		ReleaseSRWLockExclusive(&_SessionMapLock);

	_pLog._dwClientCount--;
}

// 받으면 그 데이터를 전달예정
void MonitorClientServer::OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket)
{
	_pLog._dwClientRecvMessageTPS++;

	// 타입 체크 후, 그 데이터를 Manager에 전달
	WORD type;
	(**cpacket) >> type;

	switch ((en_PACKET_TYPE)type)
	{
	case en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN:
		// Session -> User (Monitor Client)
		MessageProc_MonitorClientLogin(cpacket, sessionID);
		break;
	}
}

void MonitorClientServer::OnError(int errorcode, WCHAR* message)
{

}