#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "LogManager.h"
#include "ProcademyProfiler.h"

void ChatServer::MessageProc()
{
	unordered_set<ULONGLONG> pendingSessionIDSet;

	int loopCnt = _MessageQ->Size();
	for (int i = 0; i < loopCnt; i++)
	{
		RefCountPointer cPacket;
		_MessageQ->Dequeue(cPacket);
		_pLog._dwUpdateQSize--;

		WORD workType;
		(**cPacket) >> workType;
		
		if (workType == en_WORK_PACKET)
		{
			PacketProc(cPacket, &pendingSessionIDSet);
		}
		else
		{
			WorkProc(cPacket, workType);
		}
	}

	for(auto it = pendingSessionIDSet.begin(); it != pendingSessionIDSet.end(); it++)
	{
		SendPost((*it));
	}
}

void ChatServer::WorkProc(RefCountPointer& cPacket, WORD workType)
{
	ULONGLONG sessionID;
	(**cPacket) >> sessionID;

	if (workType == en_WORK_ACCEPT)
	{
		st_SESSION* pSession = _SessionPool->Alloc();

		pSession->ulSessionID = sessionID;
		pSession->bDeleted = FALSE;
		pSession->dwLastRecvTime = timeGetTime();

		_SessionMap.insert({ sessionID, pSession });
		_pLog._dwSessionCount++;
	}
	else // en_WORK_RELEASE
	{
		auto itUser = _UserMap.find(sessionID);
		if (itUser != _UserMap.end())
		{
			st_USER* pUser = (*itUser).second;

			vector<st_USER*>& refSectorVector = _SectorVector[pUser->sectorY][pUser->sectorX];
			for (int i = 0; i < refSectorVector.size(); i++)
			{
				if (refSectorVector[i]->ulSessionID == pUser->ulSessionID)
				{
					refSectorVector.erase(refSectorVector.begin() + i);
					break;
				}
			}

			_UserMap.erase(sessionID);
			_AccountNumUserMap.erase(pUser->AccountNum);

			_UserPool->Free(pUser);

			_pLog._dwUserCount--;
			_pLog._dwPlayerPoolUse--;
		}

		auto itSession = _SessionMap.find(sessionID);
		if (itSession != _SessionMap.end())
		{
			st_SESSION* pSession = (*itSession).second;

			_SessionMap.erase(sessionID);
			_SessionPool->Free(pSession);

			_pLog._dwSessionCount--;
		}
	}

	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;
}


void ChatServer::PacketProc(RefCountPointer& cPacket, unordered_set<ULONGLONG>* pendingIDSet)
{
	ULONGLONG sessionID;
	(**cPacket) >> sessionID;

	WORD type;
	(**cPacket) >> type;

	INT64 AccountNo;
	(**cPacket) >> AccountNo;


	// enum에 따라 다른 메세지 처리 ... 추가 예정
	switch ((en_PACKET_TYPE)type)
	{
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		MessageProc_Message(cPacket, AccountNo, sessionID, pendingIDSet);
		break;
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		MessageProc_Login(cPacket, AccountNo, sessionID);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		MessageProc_Move(cPacket, AccountNo, sessionID, pendingIDSet);
		break;
	default:
		if(!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		break;
	}
}

void ChatServer::MessageProc_Login(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	BYTE status = TRUE;
	auto it = _AccountNumUserMap.find(accountNum);
	if (it != _AccountNumUserMap.end())
	{
		// @@TODO : 중복 로그인이니 둘 다 끊어야 한다.
		_pLog._dwDuplicatedLoginTotal++;
		Disconnect((*it).second->ulSessionID);
		status = FALSE;

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

	auto itSession = _SessionMap.find(sessionID);
	if (itSession != _SessionMap.end())
	{
		_SessionMap.erase(userPtr->ulSessionID);
		_SessionPool->Free((*itSession).second);
	}
	
	_UserMap.insert({ userPtr->ulSessionID, userPtr });
	_AccountNumUserMap.insert({ userPtr->AccountNum, userPtr });

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

void ChatServer::MessageProc_Move(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID, unordered_set<ULONGLONG>* pendingIDSet)
{
	auto it = _UserMap.find(sessionID);
	if (it == _UserMap.end())
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		return;
	}

	WORD nSectorX;
	WORD nSectorY;

	(**cPacket) >> nSectorX;
	(**cPacket) >> nSectorY;

	// 첫 Move 호출은 섹터에 넣기만
	if ((*it).second->bBatched == FALSE)
	{
		(*it).second->bBatched = TRUE;
	}
	else
	{
		WORD sectorX = (*it).second->sectorX;
		WORD sectorY = (*it).second->sectorY;

		// 기존 벡터에서 삭제
		vector<st_USER*>& refSectorVector = _SectorVector[sectorY][sectorX];
		for (int i = 0; i < refSectorVector.size(); i++)
		{
			if (refSectorVector[i]->AccountNum == (*it).second->AccountNum)
			{
				refSectorVector.erase(refSectorVector.begin() + i);
				break;
			}
		}
	}
	
	(*it).second->sectorX = nSectorX;
	(*it).second->sectorY = nSectorY;
	(*it).second->dwLastRecvTime = timeGetTime();

	// 추가
	_SectorVector[nSectorY][nSectorX].push_back((*it).second);

	_pLog._dwMoveMessageTPS++;

	// MoveRES 보내기
	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESSectorMove(cPacket, accountNum, nSectorX, nSectorY);

	//PostPacket(sessionID, cPacket);
	if (!EnqueueSendBuffer(sessionID, cPacket))
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
	}
	else
		pendingIDSet->insert(sessionID);
}

void ChatServer::MessageProc_Message(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID, unordered_set<ULONGLONG>* pendingIDSet)
{
	auto it = _UserMap.find(sessionID);
	if (it == _UserMap.end())
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		return;
	}

	WORD sectorX = (*it).second->sectorX;
	WORD sectorY = (*it).second->sectorY;
	(*it).second->dwLastRecvTime = timeGetTime();

	int idx = 0;
	
	// MessageRES 보내기
	WORD len;
	WCHAR message[200];

	(**cPacket) >> len;
	(*cPacket)->GetData((char*)message, sizeof(WCHAR) * len);

	// 메세지를 초기화해서 재사용, 인코딩하기
	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESMessage(cPacket, accountNum, (*it).second->ID, (*it).second->NickName, len, message);

	st_NetHeader netHeader;
	netHeader.FixedKey = PROGRAM_KEY;
	netHeader.RandKey = (unsigned char)rand() % 256;
	netHeader.shLen = (*cPacket)->GetDataSize();

	(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
	(*cPacket)->Encode(FIXED_KEY, netHeader.RandKey);

	// 자신도 포함해서 주변 섹터의 ulSessionID 배열에 추가
	int sendCount = 0;
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
				cPacket.IncRefCount();
				if (!EnqueueSendBuffer(refSectorVector[i]->ulSessionID, cPacket, false))
				{
					if (!cPacket.DecRefCount())
						_pLog._dwPacketPoolUse--;
				}
				else
					pendingIDSet->insert(refSectorVector[i]->ulSessionID);
			}
		}
	}

	_pLog._dwChatMessageTPS++;
	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;
	//printf("\n\n--setTime : %d--refTime : %d--sendMultiTime:%d--\n\n", setTime, refCountTime, sendMultiTime);
}