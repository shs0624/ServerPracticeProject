#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "LogManager.h"

void ChatServer::MessageProc_Login(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	BYTE status = TRUE;
	AcquireSRWLockExclusive(&_AccountNumUserMapLock);
	auto it = _AccountNumUserMap.find(accountNum);
	if (it != _AccountNumUserMap.end())
	{
		// 둘 다 끊어버리겠다.
		ULONGLONG _aliveSessionID = (*it).second->ulSessionID;
		ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

		_pLog._dwDuplicatedLoginTotal++;
		Disconnect(_aliveSessionID);

		status = FALSE;

		// 새로운 유저 - status False 반환
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status, accountNum);

		if (!SendPacket_UniCast(sessionID, cPacket))
		{
			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;
		}

		Disconnect(sessionID);
		return;
	}
	ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

	WCHAR tempID[20];
	WCHAR tempNickname[20];
	CHAR tempSessionKey[64];

	(*cPacket)->GetData((char*)tempID, sizeof(tempID));
	(*cPacket)->GetData((char*)tempNickname, sizeof(tempNickname));
	(*cPacket)->GetData((char*)tempSessionKey, sizeof(tempSessionKey));

	// Redis 검증
	cpp_redis::client& _redisClient = GetTLSRedisClient();
	//cpp_redis::client* _redisClient = _pRedisClient;

	cpp_redis::reply reply;
	try {
		auto fut = _redisClient.get(std::to_string(accountNum));
		_redisClient.sync_commit();       
		reply = fut.get();                // future_error 가능
	}
	catch (const std::exception& e) {
		// Redis 통신 실패 처리
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status, accountNum);

		if (!SendPacket_UniCast(sessionID, cPacket))
		{
			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;
		}

		_pLog._dwRedisCertificationFailTotal++;
		Disconnect(sessionID);
		return;
	}

	if (!reply.is_string()) {
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status, accountNum);

		if (!SendPacket_UniCast(sessionID, cPacket))
		{
			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;
		}

		_pLog._dwRedisCertificationFailTotal++;
		Disconnect(sessionID);
		return;
	}

	if (strncmp(tempSessionKey, reply.as_string().c_str(), 64) != 0)// 요청 전송
	{
		// 검증 실패
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status, accountNum);

		if (!SendPacket_UniCast(sessionID, cPacket))
		{
			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;
		}

		_pLog._dwRedisCertificationFailTotal++;
		Disconnect(sessionID);
		return;
	}

	st_USER* userPtr = _UserPool->Alloc();

	_pLog._dwPlayerPoolUse++;
	
	userPtr->ulSessionID = sessionID;
	userPtr->AccountNum = accountNum;
	userPtr->dwLastRecvTime = timeGetTime();
	userPtr->bBatched = FALSE;
	wcsncpy_s(userPtr->ID, tempID, sizeof(WCHAR) * 20);
	wcsncpy_s(userPtr->NickName, tempNickname, sizeof(WCHAR) * 20);
	memcpy_s(userPtr->SessionKey, sizeof(userPtr->SessionKey), tempSessionKey, sizeof(tempSessionKey));

	AcquireSRWLockExclusive(&_SessionMapLock);
	auto itSession = _SessionMap.find(sessionID);
	if (itSession != _SessionMap.end())
	{
		st_SESSION* ptr = (*itSession).second;

		userPtr->ClientAddr = ptr->ClientAddr;
		_SessionMap.erase(sessionID);
		_SessionPool->Free(ptr);
	}
	ReleaseSRWLockExclusive(&_SessionMapLock);

	AcquireSRWLockExclusive(&_UserMapLock);
	_UserMap.insert({ userPtr->ulSessionID, userPtr });
	ReleaseSRWLockExclusive(&_UserMapLock);

	AcquireSRWLockExclusive(&_AccountNumUserMapLock);
	_AccountNumUserMap.insert({ userPtr->AccountNum, userPtr });
	ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

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
	if (it == _UserMap.end())
	{
		ReleaseSRWLockShared(&_UserMapLock);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		return;
	}

	WORD nSectorX;
	WORD nSectorY;

	(**cPacket) >> nSectorX;
	(**cPacket) >> nSectorY;

	if ((*it).second->bBatched == FALSE)
	{
		(*it).second->bBatched = TRUE;

		(*it).second->sectorX = nSectorX;
		(*it).second->sectorY = nSectorY;
		(*it).second->dwLastRecvTime = timeGetTime();

		AcquireSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
		_SectorVector[nSectorY][nSectorX].push_back((*it).second->ulSessionID);
		ReleaseSRWLockExclusive(&_SectorLock[nSectorY][nSectorX]);
	}
	else
	{
		WORD sectorX = (*it).second->sectorX;
		WORD sectorY = (*it).second->sectorY;

		// 이동할 때 이동 대상이 사라지는 타이밍을 막을 필요가 있음.
		// 숫자가 작은 순서대로 락을 걸게 만들자. A->B, B->A 둘 다 A먼저 락을 걸게 만드는 것.
		LockSectorMove(sectorX, sectorY, nSectorX, nSectorY);

		// 기존 벡터에서 삭제
		vector<ULONGLONG>& refSectorVector = _SectorVector[sectorY][sectorX];
		bool bFlag = false;
		for (int i = 0; i < refSectorVector.size(); i++)
		{
			if (refSectorVector[i] == (*it).second->ulSessionID)
			{
				refSectorVector.erase(refSectorVector.begin() + i);
				bFlag = true;
				break;
			}
		}
		if (bFlag == false)
			DebugBreak();

		(*it).second->sectorX = nSectorX;
		(*it).second->sectorY = nSectorY;
		(*it).second->dwLastRecvTime = timeGetTime();

		_SectorVector[nSectorY][nSectorX].push_back((*it).second->ulSessionID);

		UnLockSectorMove(sectorX, sectorY, nSectorX, nSectorY);
	}

	ReleaseSRWLockShared(&_UserMapLock);
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
	if (it == _UserMap.end())
	{
		ReleaseSRWLockShared(&_UserMapLock);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		return;
	}

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

	ReleaseSRWLockShared(&_UserMapLock);

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

	// 자신 포함해서 다 보냈으니 1을 줄여야 짝이 맞는다.
	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;

	_pLog._dwChatMessageTPS++;
}

// 함수 내부적으로 공유 락을 걸고있음.
void ChatServer::SendPacket_Sector(RefCountPointer& cPacket, WORD sectorX, WORD sectorY)
{
	AcquireSRWLockShared(&_SectorLock[sectorY][sectorX]);
	vector<ULONGLONG>& refSectorVector = _SectorVector[sectorY][sectorX];
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		cPacket.IncRefCount();
		SendPacket_UniCast(refSectorVector[i], cPacket, false);
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