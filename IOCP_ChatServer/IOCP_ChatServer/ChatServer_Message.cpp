#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "LogManager.h"

void ChatServer::MessageProc()
{
	int loopCnt = _MessageQ->Size();
	for (int i = 0; i < loopCnt; i++)
	{
		RefCountPointer cPacket;
		_MessageQ->Dequeue(cPacket);

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
		LogController::_LogController._dwSessionCount++;
	}
	else // en_WORK_RELEASE
	{
		bool bUser = FALSE;

		auto itUser = _UserMap.find(sessionID);
		if (itUser == _UserMap.end())
			bUser = FALSE;

		auto itSession = _SessionMap.find(sessionID);
		if (itSession == _SessionMap.end())
			return;//DebugBreak();
		
		if (bUser)
		{
			st_USER* pUser = (*itUser).second;

			vector<st_USER*>& refSectorVector = _SectorVector[pUser->sectorY][pUser->sectorX];
			for (int i = 0; i < refSectorVector.size(); i++)
			{
				if (refSectorVector[i]->ulSessionID == pUser->ulSessionID)
				{
					refSectorVector.erase(refSectorVector.begin() + i);
					return;
				}
			}

			_UserMap.erase(sessionID);
			_UserPool->Free(pUser);

			LogController::_LogController._dwUserCount--;
			LogController::_LogController._dwPlayerPoolUse--;
		}
		else
		{
			st_SESSION* pSession = (*itSession).second;

			_SessionMap.erase(sessionID);
			_SessionPool->Free(pSession);

			LogController::_LogController._dwSessionCount--;
		}
	}
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
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		MessageProc_Login(cPacket, AccountNo, sessionID);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		MessageProc_Move(cPacket, AccountNo, sessionID);
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		MessageProc_Message(cPacket, AccountNo, sessionID);
		break;
	}

	// @@TODO : 없으면, 이상한건데 그거에 대한 처리
	// unordered_map<INT64, st_CHARACTER*>::iterator
	auto it = _UserMap.find(sessionID);
	if (it == _UserMap.end())
		return;

	(*it).second->dwLastRecvTime = timeGetTime();
}

void ChatServer::MessageProc_Login(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	BYTE status = TRUE;
	auto it = _UserMap.find(sessionID);
	if (it != _UserMap.end())
	{
		// @@TODO : 중복 로그인이니 둘 다 끊어야 한다.
		status = FALSE;
	}

	st_USER* userPtr = _UserPool->Alloc();
	LogController::_LogController._dwPlayerPoolUse++;
	
	userPtr->ulSessionID = sessionID;
	userPtr->AccountNum = accountNum;
	userPtr->dwLastRecvTime = timeGetTime();
	userPtr->bBatched = FALSE;
	
	(*cPacket)->GetData((char*)userPtr->ID, sizeof(userPtr->ID));
	(*cPacket)->GetData((char*)userPtr->NickName, sizeof(userPtr->NickName));
	(*cPacket)->GetData((char*)userPtr->SessionKey, sizeof(userPtr->SessionKey));

	_SessionMap.erase(userPtr->ulSessionID);
	_UserMap.insert({ userPtr->ulSessionID, userPtr });

	LogController::_LogController._dwSessionCount--;
	LogController::_LogController._dwUserCount++;
	LogController::_LogController._dwLoginMessageTPS++;

	// LoginRES 보내기
	cPacket.DecRefCount();

	RefCountPointer sendPacket = RefCountPointer::MakeSharedPtr();
	(*sendPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));

	mpRESLogin(sendPacket, status, accountNum);

	SendPacket_UniCast(sessionID, sendPacket);
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

	// 추가
	_SectorVector[nSectorY][nSectorX].push_back((*it).second);
	if ((*it).second->bBatched == FALSE)
		(*it).second->bBatched = TRUE;

	LogController::_LogController._dwMoveMessageTPS++;

	// MoveRES 보내기
	cPacket.DecRefCount();

	RefCountPointer sendPacket = RefCountPointer::MakeSharedPtr();
	(*sendPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));

	mpRESSectorMove(sendPacket, accountNum, nSectorX, nSectorY);

	SendPacket_UniCast(sessionID, sendPacket);
}

void ChatServer::MessageProc_Message(RefCountPointer& cPacket, INT64 accountNum, ULONGLONG sessionID)
{
	//@@TODO : 평균적인 수치 알아내서 크기 줄이기
	ULONGLONG sessionIDArray[4000];

	auto it = _UserMap.find(sessionID);
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

	//(*cPacket)->GetData((char*)id, sizeof(id));
	//(*cPacket)->GetData((char*)nick, sizeof(nick));
	memcpy(id, (*it).second->ID, sizeof(id));
	memcpy(nick, (*it).second->NickName, sizeof(nick));
	(**cPacket) >> len;
	(*cPacket)->GetData((char*)message, sizeof(WCHAR) * len);

	/*(*cPacket)->Clear();
	(*cPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));*/

	cPacket.DecRefCount();

	RefCountPointer sendPacket = RefCountPointer::MakeSharedPtr();
	(*sendPacket)->Initialize(PROTOCOL_MAX_SIZE, sizeof(st_NetHeader));
	mpRESMessage(sendPacket, accountNum, id, nick, len, message);

	SendPacket_MultiCast(sessionIDArray, idx, sendPacket);
}