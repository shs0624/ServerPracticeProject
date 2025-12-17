#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "CFreeList_LockFree.h"
#include "LogManager.h"



void ChatServer::InitChatServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	int workCount = (int)si.dwNumberOfProcessors * 2;
	StartNetServer(ip, port, workCount, workCount - 2, true, maxConnection);

	InitializeSRWLock(&_UserMapLock);
	InitializeSRWLock(&_SessionMapLock);

	for (int iY = 0; iY < dfSECTOR_MAX_Y; iY++)
	{
		for (int iX = 0; iX < dfSECTOR_MAX_X; iX++)
		{
			InitializeSRWLock(&_SectorLock[iY][iX]);
		}
	}

	_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	_hTimeoutEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

	_UserPool = new procademy::CMemoryPool_LockFree<st_USER>(maxConnection, false, false);
	_SessionPool = new procademy::CMemoryPool_LockFree<st_SESSION>(maxConnection, false, false);

	//_TimerThreadHandle = (HANDLE)_beginthreadex(NULL, 0, TimerThread, this, 0, &_TimerThreadID);
}

// 필요할 때 초기화 해서 사용할 수 있는 함수
cpp_redis::client& ChatServer::GetTLSRedisClient()
{
	thread_local cpp_redis::client client;
	thread_local bool connected = false;

	if (!connected) {
		client.connect();
		connected = true;
	}

	return client;
}


bool ChatServer::OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr)
{
	st_SESSION* pSession = _SessionPool->Alloc();

	pSession->ulSessionID = sessionID;
	pSession->ClientAddr = clientAddr;
	pSession->dwLastRecvTime = timeGetTime();

	AcquireSRWLockExclusive(&_SessionMapLock);
	_SessionMap.insert({ sessionID, pSession });
	ReleaseSRWLockExclusive(&_SessionMapLock);

	_pLog._dwSessionCount++;

	return true;
}

void ChatServer::OnRelease(ULONGLONG sessionID)
{
	// 세션 Release
	AcquireSRWLockExclusive(&_UserMapLock);
	auto itUser = _UserMap.find(sessionID);
	if (itUser != _UserMap.end())
	{
		st_USER* pUser = (*itUser).second;

		if (pUser->bBatched != FALSE)
		{
			AcquireSRWLockExclusive(&_SectorLock[pUser->sectorY][pUser->sectorX]);
			vector<ULONGLONG>& refSectorVector = _SectorVector[pUser->sectorY][pUser->sectorX];
			for (int i = 0; i < refSectorVector.size(); i++)
			{
				if (refSectorVector[i] == pUser->ulSessionID)
				{
					refSectorVector.erase(refSectorVector.begin() + i);
					break;
				}
			}
			ReleaseSRWLockExclusive(&_SectorLock[pUser->sectorY][pUser->sectorX]);
		}

		_UserMap.erase(sessionID);
		ReleaseSRWLockExclusive(&_UserMapLock);

		AcquireSRWLockExclusive(&_AccountNumUserMapLock);
		_AccountNumUserMap.erase(pUser->AccountNum);
		ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

		_UserPool->Free(pUser);

		_pLog._dwUserCount--;
		_pLog._dwPlayerPoolUse--;
	}
	else
		ReleaseSRWLockExclusive(&_UserMapLock);

	AcquireSRWLockExclusive(&_SessionMapLock);
	auto itSession = _SessionMap.find(sessionID);
	if (itSession != _SessionMap.end())
	{
		st_SESSION* pSession = (*itSession).second;
		_SessionMap.erase(sessionID);
		_SessionPool->Free(pSession);

		ReleaseSRWLockExclusive(&_SessionMapLock);

		_pLog._dwSessionCount--;
	}
	else
		ReleaseSRWLockExclusive(&_SessionMapLock);
}

void ChatServer::OnRecv(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	WORD type;
	(**cPacket) >> type;

	INT64 AccountNo;
	(**cPacket) >> AccountNo;

	// enum에 따라 다른 메세지 처리
	switch ((en_PACKET_TYPE)type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		MessageProc_Login(cPacket, AccountNo, sessionID);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		MessageProc_Move(cPacket, AccountNo, sessionID);
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		MessageProc_Message(cPacket, AccountNo, sessionID);
		break;
	default:
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		break;
	}
}

void ChatServer::OnError(int errorcode, WCHAR* message)
{

}