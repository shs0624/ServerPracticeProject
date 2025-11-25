#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "LogManager.h"

void ChatServer::MessageProc_Login(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	BYTE status = TRUE;
	AcquireSRWLockShared(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	if (it != _UserMap.end())
	{
		// @@TODO : 중복 로그인이니 둘 다 끊어야 한다.
		ReleaseSRWLockShared(&_UserMapLock);
		DebugBreak();
		status = FALSE;
		return;
	}
	ReleaseSRWLockShared(&_UserMapLock);

	st_USER* userPtr = _UserPool->Alloc();
	LogController::_LogController._dwPlayerPoolUse++;
	
	userPtr->ulSessionID = sessionID;
	userPtr->AccountNum = accountNum;
	userPtr->dwLastRecvTime = timeGetTime();
	userPtr->bDeleted = FALSE;
	userPtr->bBatched = FALSE;
	
	(*cPacket)->GetData((char*)userPtr->ID, sizeof(userPtr->ID));
	(*cPacket)->GetData((char*)userPtr->NickName, sizeof(userPtr->NickName));
	(*cPacket)->GetData((char*)userPtr->SessionKey, sizeof(userPtr->SessionKey));

	AcquireSRWLockExclusive(&_SessionMapLock);
	auto itSession = _SessionMap.find(sessionID);
	if (itSession != _SessionMap.end())
	{
		st_SESSION* ptr = (*itSession).second;

		_SessionMap.erase(userPtr->ulSessionID);
		_SessionPool->Free(ptr);
	}
	ReleaseSRWLockExclusive(&_SessionMapLock);

	AcquireSRWLockExclusive(&_UserMapLock);
	_UserMap.insert({ userPtr->ulSessionID, userPtr });
	ReleaseSRWLockExclusive(&_UserMapLock);

	LogController::_LogController._dwSessionCount--;
	LogController::_LogController._dwUserCount++;
	LogController::_LogController._dwLoginMessageTPS++;

	// LoginRES 보내기
	cPacket.DecRefCount();

	RefCountPointer sendPacket = RefCountPointer::MakeSharedPtr();
	(*sendPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));
	InterlockedIncrement(&LogController::_LogController._dwPacketPoolUse);

	mpRESLogin(sendPacket, status, accountNum);

	SendPacket_UniCast(sessionID, sendPacket);
}

void ChatServer::MessageProc_Move(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	AcquireSRWLockShared(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	ReleaseSRWLockShared(&_UserMapLock);

	if (it == _UserMap.end())
	{
		// @@TODO : 없는 유저에 대한 메세지. 오류 로그를 남겨야 할듯
		return;
	}

	WORD nSectorX;
	WORD nSectorY;

	(**cPacket) >> nSectorX;
	(**cPacket) >> nSectorY;

	WORD sectorX = (*it).second->sectorX;
	WORD sectorY = (*it).second->sectorY;

	// 기존 벡터에서 삭제
	vector<st_USER*>& refSectorVector = _SectorVector[sectorY][sectorX];

	AcquireSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		if (refSectorVector[i]->AccountNum == (*it).second->AccountNum)
		{
			refSectorVector.erase(refSectorVector.begin() + i);
			break;
		}
	}
	ReleaseSRWLockExclusive(&_SectorLock[sectorY][sectorX]);

	(*it).second->sectorX = nSectorX;
	(*it).second->sectorY = nSectorY;

	// @@TODO : 추가는 락이 필요할까?
	AcquireSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
	_SectorVector[nSectorY][nSectorX].push_back((*it).second);
	ReleaseSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);

	if ((*it).second->bBatched == FALSE)
		(*it).second->bBatched = TRUE;

	LogController::_LogController._dwMoveMessageTPS++;

	// MoveRES 보내기
	cPacket.DecRefCount();

	RefCountPointer sendPacket = RefCountPointer::MakeSharedPtr();
	(*sendPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));
	InterlockedIncrement(&LogController::_LogController._dwPacketPoolUse);

	mpRESSectorMove(sendPacket, accountNum, nSectorX, nSectorY);

	SendPacket_UniCast(sessionID, sendPacket);
}

void ChatServer::MessageProc_Message(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	//@@TODO : 평균적인 수치 알아내서 크기 줄이기
	ULONGLONG sessionIDArray[4000];

	AcquireSRWLockShared(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	ReleaseSRWLockShared(&_UserMapLock);

	if (it == _UserMap.end())
	{
		// @@TODO : 없는 유저에 대한 메세지. 오류 로그를 남겨야 할듯
		return;
	}

	if ((*it).second->bBatched == FALSE)
		DebugBreak();

	WORD sectorX = (*it).second->sectorX;
	WORD sectorY = (*it).second->sectorY;

	int idx = 0;
	
	// 자신도 포함해서 주변 섹터의 ulSessionID 배열에 추가
	for (int iY = -1; iY <= 1; iY++)
	{
		if (sectorY + iY < 0 || sectorY + iY >= dfSECTOR_MAX_Y)
			continue;

		for (int iX = -1; iX <= 1; iX++)
		{
			if (sectorX + iX < 0 || sectorX + iX >= dfSECTOR_MAX_X)
				continue;

			vector<st_USER*>& refSectorVector = _SectorVector[sectorY + iY][sectorX + iX];
			for (int i = 0; i < refSectorVector.size(); i++)
			{
				sessionIDArray[idx++] = refSectorVector[i]->ulSessionID;
			}
		}
	}

	if (idx > 4000)
		DebugBreak();

	LogController::_LogController._dwChatMessageTPS++;

	// MessageRES 보내기
	WCHAR id[20];
	WCHAR nick[20];
	WORD len;
	WCHAR message[200];

	memcpy(id, (*it).second->ID, sizeof(id));
	memcpy(nick, (*it).second->NickName, sizeof(nick));
	(**cPacket) >> len;
	(*cPacket)->GetData((char*)message, sizeof(WCHAR) * len);

	cPacket.DecRefCount();

	RefCountPointer sendPacket = RefCountPointer::MakeSharedPtr();
	(*sendPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));
	InterlockedIncrement(&LogController::_LogController._dwPacketPoolUse);

	mpRESMessage(sendPacket, accountNum, id, nick, len, message);

	SendPacket_MultiCast(sessionIDArray, idx, sendPacket);
}