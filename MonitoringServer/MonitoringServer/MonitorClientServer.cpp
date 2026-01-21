#include "Includes.h"
#include "NetServer.h"
#include "LanServer.h"
#include "LogManager.h"
#include "MonitorProtocol.h"
#include "MonitorClientServer.h"
#include "MonitorDataManager.h"
#include "MonitorChatServer.h"

void MonitorClientServer::InitMonitorClientServer(ULONG ip, int maxConnection, unsigned char programKey, unsigned char fixedKey)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	int workCount = (int)si.dwNumberOfProcessors * 2;
	StartNetServer(ip, dfSERVERPORT_CLIENT, workCount / 2, true, maxConnection, programKey, fixedKey);

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
	default:
		if (!cpacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		break;
	}
}

// IOCP가 아닌 MonitorDataManager가 호출하기 때문에, 그 로그의 TLS 로그주소 넘겨받기
void MonitorClientServer::Update(BYTE serverNum, BYTE dataType, int dataValue, int timeStamp, stChatLog* pLog)
{
	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	(*cPacket)->Initialize(sizeof(st_NetHeader));
	pLog->_dwPacketPoolUse++;

	mpDataUpdate(cPacket, serverNum, dataType, dataValue, timeStamp);

	// 직접 패킷 인코딩까지
	st_NetHeader lanHeader;
	lanHeader.FixedKey = _ProgramKey;
	lanHeader.RandKey = (unsigned char)rand() % 256;
	lanHeader.shLen = (*cPacket)->GetDataSize();

	(*cPacket)->PushHeader((char*)&lanHeader, sizeof(st_NetHeader));
	(*cPacket)->Encode(_FixedKey, lanHeader.RandKey);

	// 연결된 클라에게 전송
	for (auto it = _UserMap.begin(); it != _UserMap.end(); it++)
	{
		cPacket.IncRefCount();
		SendPacket_UniCast((*it).second->ulSessionID, cPacket, false);
	}

	if (!cPacket.DecRefCount())
		pLog->_dwPacketPoolUse--;
	return;
}

void MonitorClientServer::OnError(int errorcode, WCHAR* message)
{

}

// time 측정을 위한 함수
void MonitorClientServer::TimeCheck()
{
	AcquireSRWLockShared(&_SessionMapLock);
	for (auto it = _SessionMap.begin(); it != _SessionMap.end(); it++)
	{
		st_ClientSESSION* pSession = (*it).second;
		DWORD timeDiff = timeGetTime() - pSession->dwLastRecvTime;
		if (timeDiff >= dfTIMEOUT_ClientSESSION)
		{
			Disconnect(pSession->ulSessionID);
			_pLog._dwTimeoutSessionTotal++;
			continue;
		}
	}
	ReleaseSRWLockShared(&_SessionMapLock);
}

unsigned int WINAPI MonitorClientServer::TimerThread(LPVOID arg)
{
	MonitorClientServer* thisPtr = (MonitorClientServer*)arg;

	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	HANDLE hHandleArr[2] = { thisPtr->_hQuitEvent, thisPtr->_hTimeoutEvent };

	DWORD ret = 0;
	while (1)
	{
		thisPtr->TimeCheck();

		ret = WaitForMultipleObjects(2, hHandleArr, FALSE, dfSLEEPTIME);
		if (ret == WAIT_OBJECT_0)
		{
			// 서버 종료
			return 0;
		}
	}
}
