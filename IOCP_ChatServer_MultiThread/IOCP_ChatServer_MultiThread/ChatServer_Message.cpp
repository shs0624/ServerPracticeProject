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
	_pLog._dwPlayerPoolUse++;
	
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

	_pLog._dwSessionCount--;
	_pLog._dwUserCount++;
	_pLog._dwLoginMessageTPS++;

	// LoginRES 보내기
	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESLogin(cPacket, status, accountNum);

	if (!SendPacket_UniCast(sessionID, cPacket))
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
	}
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
	vector<ULONGLONG>& refSectorVector = _SectorVector[sectorY][sectorX];

	// 이동할 때 이동 대상이 사라지는 타이밍을 막을 필요가 있음.
	// 숫자가 작은 순서대로 락을 걸게 만들자. A->B, B->A 둘 다 A먼저 락을 걸게 만드는 것.
	LockSectorMove(sectorX, sectorY, nSectorX, nSectorY);

	// 기존 섹터에서의 제거
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		if (refSectorVector[i] == (*it).second->ulSessionID)
		{
			refSectorVector.erase(refSectorVector.begin() + i);
			break;
		}
	}

	(*it).second->sectorX = nSectorX;
	(*it).second->sectorY = nSectorY;
	(*it).second->dwLastRecvTime = timeGetTime();

	_SectorVector[nSectorY][nSectorX].push_back((*it).second->ulSessionID);

	UnLockSectorMove(sectorX, sectorY, nSectorX, nSectorY);

	if ((*it).second->bBatched == FALSE)
		(*it).second->bBatched = TRUE;

	_pLog._dwMoveMessageTPS++;

	// MoveRES 보내기
	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESSectorMove(cPacket, accountNum, nSectorX, nSectorY);

	if (!SendPacket_UniCast(sessionID, cPacket))
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
	}
}

void ChatServer::MessageProc_Message(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	//@@TODO : 평균적인 수치 알아내서 크기 줄이기
	ULONGLONG sessionIDArray[1000];

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

	(*it).second->dwLastRecvTime = timeGetTime();
	WORD sectorX = (*it).second->sectorX;
	WORD sectorY = (*it).second->sectorY;

	int idx = 0;

	// MessageRES 보내기
	WORD len;
	WCHAR message[200];
	(**cPacket) >> len;
	(*cPacket)->GetData((char*)message, sizeof(WCHAR) * len);

	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESMessage(cPacket, accountNum, (*it).second->ID, (*it).second->NickName, len, message);
	
	// 메세지를 먼저 생성, 인코딩하기
	st_NetHeader netHeader;
	netHeader.FixedKey = PROGRAM_KEY;
	netHeader.RandKey = (unsigned char)rand() % 256;
	netHeader.shLen = (*cPacket)->GetDataSize();

	(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
	(*cPacket)->Encode(FIXED_KEY, netHeader.RandKey);

	// 주변 섹터에 락걸며 차례대로 메세지 전송
	for (int iY = -1; iY <= 1; iY++)
	{
		if (sectorY + iY < 0 || sectorY + iY >= dfSECTOR_MAX_Y)
			continue;

		for (int iX = -1; iX <= 1; iX++)
		{
			if (sectorX + iX < 0 || sectorX + iX >= dfSECTOR_MAX_X)
				continue;

			SendPacket_Sector(cPacket, sectorX + iX, sectorY + iY);
		}
	}

	//SendPacket_UniCast(sessionID, cPacket, false);

	// 자신 포함해서 다 보냈으니 1을 줄여야 짝이 맞는다.
	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;

	_pLog._dwChatMessageTPS++;
}

// 함수 내부적으로 공유 락을 걸고있음.
void ChatServer::SendPacket_Sector(RefCountPointer& cPacket, WORD sectorX, WORD sectorY)
{
	unordered_set<ULONGLONG> pendingSessionIDSet;

	AcquireSRWLockShared(&_SectorLock[sectorY][sectorX]);
	vector<ULONGLONG>& refSectorVector = _SectorVector[sectorY][sectorX];
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		cPacket.IncRefCount();
		// @@TODO : 이걸 보내다가 중간에 연결이 끊길 수 있는데, 그럼 벡터 이터레이터가 무너짐.
		// 추가로 데드락도 발생 가능하다. OnRelease에서 섹터 락을 잡으니까. 암튼 여기서 삭제하면 안됨.
		if (!SendPacket_UniCast(refSectorVector[i], cPacket, false))
		{
			/*if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;*/
		}
	}
	ReleaseSRWLockShared(&_SectorLock[sectorY][sectorX]);
}

// 락에 규칙을 정하자. X가 작은 거 먼저걸고, Y도 작은거 먼저 걸자.
void ChatServer::LockSectorMove(WORD sectorX, WORD sectorY, WORD nSectorX, WORD nSectorY)
{
	if (sectorX == nSectorX && sectorY == nSectorY)
	{
		AcquireSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
		return;
	}

	if (sectorX < nSectorX)
	{
		if (sectorY < nSectorY)
		{
			AcquireSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
			AcquireSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
		}
		else
		{
			AcquireSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
			AcquireSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
		}
	}
	else
	{
		if (sectorY < nSectorY)
		{
			AcquireSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
			AcquireSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
		}
		else
		{
			AcquireSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
			AcquireSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
		}
	}
}

// 락을 푸는 순서는 반대다.
void ChatServer::UnLockSectorMove(WORD sectorX, WORD sectorY, WORD nSectorX, WORD nSectorY)
{
	if (sectorX == nSectorX && sectorY == nSectorY)
	{
		ReleaseSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
		return;
	}

	if (sectorX < nSectorX)
	{
		if (sectorY < nSectorY)
		{
			ReleaseSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
			ReleaseSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
		}
		else
		{
			ReleaseSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
			ReleaseSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
		}
	}
	else
	{
		if (sectorY < nSectorY)
		{
			ReleaseSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
			ReleaseSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
		}
		else
		{
			ReleaseSRWLockExclusive(&_SectorLock[sectorY][sectorX]);
			ReleaseSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
		}
	}
}