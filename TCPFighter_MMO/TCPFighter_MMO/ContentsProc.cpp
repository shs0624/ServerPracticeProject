#include <WS2tcpip.h>
#include <Windows.h>
#include <list>
#include <unordered_map>
using namespace std;

#include "PacketDefine.h"
#include "TCPDefine.h"
#include "MessageProc.h"
#include "SectorDefine.h"
#include "ContentsDefine.h"
#include "SectorProc.h"
#include "ContentsProc.h"
#include "MessageCreate.h"
#include "Debug.h"

extern unordered_map<DWORD, st_CHARACTER*> m_CharacterMap;

bool netPacketProc_MoveStart(st_SESSION* session, CPacket* packet)
{
	char csAction;
	short csX;
	short csY;

	*packet >> csAction;
	*packet >> csX;
	*packet >> csY;

	st_CHARACTER* _player = (*(m_CharacterMap.find(session->dwSessionID))).second;

	// 이건 나중에 로그로 넘기던가 해야함
	char ipbuffer[50];
	inet_ntop(AF_INET, &(session->IPPtr.sin_addr), ipbuffer, 50);
	//오차 범위 확인 -> 애초에 필요한가?
	if (abs(_player->shX - csX) > dfERROR_RANGE || abs(_player->shY - csY) > dfERROR_RANGE)
	{
		// 싱크 전송, 서버 주소로 수정
		printf("[SYNC - Packet MoveStart] Server x,y : %d, %d | Client x,y : %d, %d\n",
			_player->shX, _player->shY, csX, csY);
	}

	// 방향 변경, 좌표 변경
	_player->dwAction = csAction;
	switch (csAction)
	{
	case dfPACKET_MOVE_DIR_RR:
	case dfPACKET_MOVE_DIR_RU:
	case dfPACKET_MOVE_DIR_RD:
		_player->byDirection = dfPACKET_MOVE_DIR_RR;
		break;
	case dfPACKET_MOVE_DIR_LL:
	case dfPACKET_MOVE_DIR_LU:
	case dfPACKET_MOVE_DIR_LD:
		_player->byDirection = dfPACKET_MOVE_DIR_LL;
		break;
	}
	_player->shX = csX;
	_player->shY = csY;

	st_PACKET_HEADER header;
	CPacket* csPacket = new CPacket(PROTOCOL_MAXSIZE);
	mpMoveStart(&header, csPacket, _player->dwSessionID, _player->dwAction, csX, csY);
	SendPacket_Around(_player, &header, csPacket);

	return true;
}

bool netPacketProc_MoveStop(st_SESSION* session, CPacket* packet)
{
	char csAction;
	short csX;
	short csY;

	*packet >> csAction;
	*packet >> csX;
	*packet >> csY;

	st_CHARACTER* _player = (*(m_CharacterMap.find(session->dwSessionID))).second;

	//오차 범위 확인
	if (abs(_player->shX - csX) > dfERROR_RANGE || abs(_player->shY - csY) > dfERROR_RANGE)
	{
		// 싱크 전송, 서버 주소로 수정
		printf("[SYNC - Packet MoveStart] Server x,y : %d, %d | Client x,y : %d, %d\n",
			_player->shX, _player->shY, csX, csY);
	}
	else
	{
		_player->shX = csX;
		_player->shY = csY;
	}

	_player->dwAction = dfPACKET_MOVE_DIR_NONE;

	st_PACKET_HEADER header;
	CPacket* csPacket = new CPacket(PROTOCOL_MAXSIZE);
	mpMoveStop(&header, csPacket, _player->dwSessionID, _player->dwAction, csX, csY);
	SendPacket_Around(_player, &header, csPacket);

	return true;
}

bool netPacketProc_Attack1(st_SESSION* session)
{
	st_CHARACTER* _player = (*(m_CharacterMap.find(session->dwSessionID))).second;

	// 싱크가 필요한지 모르겠음
	


	return true;
}

bool netPacketProc_Attack2(st_SESSION* session)
{

	return true;
}

bool netPacketProc_Attack3(st_SESSION* session)
{

	return true;
}

bool netPacketProc_Echo(st_SESSION* session)
{

	return true;
}

void CollisionCheck(st_CHARACTER* pExceptPlayer, char chDir, BYTE xRange, BYTE yRange, list<st_CHARACTER*> pCheckedList)
{
	unordered_map<DWORD, st_CHARACTER*>::iterator it;

	for (it = m_CharacterMap.begin(); it != m_CharacterMap.end(); it++)
	{
		if ((*it).second->dwSessionID == pExceptPlayer->dwSessionID)
			continue;

		switch (chDir)
		{
		case dfPACKET_MOVE_DIR_LL:
			break;
		case dfPACKET_MOVE_DIR_RR:
			break;
		}
	}
}

bool netPacketProc_Accept(st_SESSION* session)
{
	st_CHARACTER* playerPtr = (st_CHARACTER*)malloc(sizeof(st_CHARACTER));
	if (playerPtr == nullptr)
	{
		err_quit("accept_malloc");
		return false;
	}

	playerPtr->pSession = session;
	playerPtr->dwSessionID = session->dwSessionID;
	playerPtr->byDirection = dfPACKET_MOVE_DIR_RR;
	playerPtr->dwAction = dfPACKET_MOVE_DIR_NONE;
	playerPtr->shX = dfRANGE_MOVE_LEFT + (rand() % (dfRANGE_MOVE_RIGHT - dfRANGE_MOVE_LEFT + 1));
	playerPtr->shY = dfRANGE_MOVE_TOP + (rand() % (dfRANGE_MOVE_BOTTOM - dfRANGE_MOVE_TOP + 1));
	playerPtr->chHP = dfHP_MAX;

	InitializeSector(playerPtr);

	m_CharacterMap.insert({ playerPtr->dwSessionID, playerPtr });

	return true;
}