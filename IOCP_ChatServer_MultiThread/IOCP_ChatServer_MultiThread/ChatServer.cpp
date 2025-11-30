#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "CFreeList.h"
#include "LogManager.h"

int main()
{
	LogController::GetInstance();

	ChatServer* _chatServer = new ChatServer(INADDR_ANY, SERVERPORT, true, 5000);

	char ch;
	while (1)
	{
		// 컨트롤?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			_chatServer->QuitServer();
			//break;
		}
		if (ch == 'P' || ch == 'p')
		{
			ProfileDataOutText("ProfileData.txt");
		}

	}
}

void ChatServer::InitChatServer()
{
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

	_TimerThreadHandle = (HANDLE)_beginthreadex(NULL, 0, TimerThread, this, 0, &_TimerThreadID);
}

bool ChatServer::OnAccept(ULONGLONG sessionID)
{
	st_SESSION* pSession = _SessionPool->Alloc();

	pSession->ulSessionID = sessionID;
	pSession->bDeleted = FALSE;
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
	AcquireSRWLockShared(&_UserMapLock);
	auto itUser = _UserMap.find(sessionID);
	ReleaseSRWLockShared(&_UserMapLock);

	if (itUser != _UserMap.end())
	{
		st_USER* pUser = (*itUser).second;

		vector<st_USER*>& refSectorVector = _SectorVector[pUser->sectorY][pUser->sectorX];
		AcquireSRWLockExclusive(&_SectorLock[pUser->sectorY][pUser->sectorX]);
		for (int i = 0; i < refSectorVector.size(); i++)
		{
			if (refSectorVector[i]->ulSessionID == pUser->ulSessionID)
			{
				refSectorVector.erase(refSectorVector.begin() + i);
				break;
			}
		}
		ReleaseSRWLockExclusive(&_SectorLock[pUser->sectorY][pUser->sectorX]);

		AcquireSRWLockExclusive(&_UserMapLock);
		_UserMap.erase(sessionID);
		ReleaseSRWLockExclusive(&_UserMapLock);

		_UserPool->Free(pUser);

		_pLog._dwUserCount--;
		_pLog._dwPlayerPoolUse--;
	}

	AcquireSRWLockShared(&_SessionMapLock);
	auto itSession = _SessionMap.find(sessionID);
	ReleaseSRWLockShared(&_SessionMapLock);

	if (itSession != _SessionMap.end())
	{
		st_SESSION* pSession = (*itSession).second;

		AcquireSRWLockExclusive(&_SessionMapLock);
		_SessionMap.erase(sessionID);
		ReleaseSRWLockExclusive(&_SessionMapLock);

		_SessionPool->Free(pSession);
		_pLog._dwSessionCount--;
	}
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