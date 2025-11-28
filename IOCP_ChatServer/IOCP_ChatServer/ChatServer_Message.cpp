#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "LogManager.h"
#include "ProcademyProfiler.h"

void ChatServer::MessageProc()
{
	int loopCnt = _MessageQ->Size();
	//printf("\n\n LoopCnt : %d\n\n", loopCnt);
	for (int i = 0; i < loopCnt; i++)
	{
		RefCountPointer cPacket;
		_MessageQ->Dequeue(cPacket);
		_pLog._dwUpdateQSize--;

		WORD workType;
		(**cPacket) >> workType;
		
		if (workType == en_WORK_PACKET)
		{
			PacketProc(cPacket);
		}
		else
		{
			WorkProc(cPacket, workType);
		}
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

	cPacket.DecRefCount();
}


void ChatServer::PacketProc(RefCountPointer& cPacket)
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
		MessageProc_Message(cPacket, AccountNo, sessionID);
		break;
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		MessageProc_Login(cPacket, AccountNo, sessionID);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		MessageProc_Move(cPacket, AccountNo, sessionID);
		break;
	default:
		cPacket.DecRefCount();
		break;
	}
}

void ChatServer::MessageProc_Login(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	BYTE status = TRUE;
	auto it = _UserMap.find(sessionID);
	if (it != _UserMap.end())
	{
		// @@TODO : 중복 로그인이니 둘 다 끊어야 한다.
		DebugBreak();
		status = FALSE;
	}

	st_USER* userPtr = _UserPool->Alloc();
	_pLog._dwPlayerPoolUse++;
	
	userPtr->ulSessionID = sessionID;
	userPtr->AccountNum = accountNum;
	userPtr->dwLastRecvTime = timeGetTime();
	userPtr->bDeleted = FALSE;
	
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

	_pLog._dwSessionCount--;
	_pLog._dwUserCount++;
	_pLog._dwLoginMessageTPS++;

	// LoginRES 보내기
	cPacket.DecRefCount();

	RefCountPointer sendPacket = RefCountPointer::MakeSharedPtr();
	(*sendPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));

	mpRESLogin(sendPacket, status, accountNum);

	PostPacket(sessionID, sendPacket);
}

void ChatServer::MessageProc_Move(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	auto it = _UserMap.find(sessionID);
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
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		if (refSectorVector[i]->AccountNum == (*it).second->AccountNum)
		{
			refSectorVector.erase(refSectorVector.begin() + i);
			break;
		}
	}

	(*it).second->sectorX = nSectorX;
	(*it).second->sectorY = nSectorY;
	(*it).second->dwLastRecvTime = timeGetTime();

	// 추가
	_SectorVector[nSectorY][nSectorX].push_back((*it).second);

	_pLog._dwMoveMessageTPS++;

	// MoveRES 보내기
	cPacket.DecRefCount();

	RefCountPointer sendPacket = RefCountPointer::MakeSharedPtr();
	(*sendPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));

	mpRESSectorMove(sendPacket, accountNum, nSectorX, nSectorY);

	PostPacket(sessionID, sendPacket);
}

void ChatServer::MessageProc_Message(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	//@@TODO : 평균적인 수치 알아내서 크기 줄이기
	auto it = _UserMap.find(sessionID);
	if (it == _UserMap.end())
	{
		// @@TODO : 없는 유저에 대한 메세지. 오류 로그를 남겨야 할듯
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

	cPacket.DecRefCount();

	RefCountPointer sendPacket = RefCountPointer::MakeSharedPtr();
	(*sendPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));
	mpRESMessage(sendPacket, accountNum, (*it).second->ID, (*it).second->NickName, len, message);

	// 메세지를 먼저 생성, 인코딩하기
	st_NetHeader netHeader;
	netHeader.FixedKey = PROGRAM_KEY;
	netHeader.RandKey = (unsigned char)rand() % 256;
	netHeader.shLen = (*sendPacket)->GetDataSize();

	(*sendPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
	(*sendPacket)->Encode(FIXED_KEY, netHeader.RandKey);

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
				sendPacket.IncRefCount();
				//@@TODO : 보내기 싫패하면 끊어야 할듯.
				if (!PostPacket(refSectorVector[i]->ulSessionID, sendPacket, false))
					sendPacket.DecRefCount();
				/*if (!SendPacket_UniCast(refSectorVector[i]->ulSessionID, sendPacket, false))
					sendPacket.DecRefCount();*/
			}
		}
	}
	_pLog._dwChatMessageTPS++;
	sendPacket.DecRefCount();
	//printf("\n\n--setTime : %d--refTime : %d--sendMultiTime:%d--\n\n", setTime, refCountTime, sendMultiTime);
}