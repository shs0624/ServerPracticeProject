#include <WS2tcpip.h>
#include <Windows.h>
#include <list>
#include <queue>
#include <unordered_map>
using namespace std;

#include "PacketDefine.h"
#include "TCPDefine.h"
#include "TCPNetwork.h"
#include "MessageProc.h"
#include "SectorDefine.h"
#include "ContentsDefine.h"
#include "SectorProc.h"
#include "CStack.h"
#include "ContentsProc.h"
#include "MessageCreate.h"
#include "Debug.h"
#include "LogProc.h"
#include "CFreeList.h"
#include "ProcademyProfiler.h"

extern int g_iLogLevel;
extern WCHAR g_szLogBuff[1024];

extern unordered_map<DWORD, st_CHARACTER*> m_CharacterMap;
extern procademy::CMemoryPool<st_CHARACTER> _CharacterPool;


bool AttackProc(st_CHARACTER* player, BYTE type, BYTE xRange, BYTE yRange, char damage);

bool SetDeleteCharacter(DWORD dwSessionID)
{
	unordered_map<DWORD, st_CHARACTER*>::iterator it;
	it = (m_CharacterMap.find(dwSessionID));
	if (it == m_CharacterMap.end())
		return false;

	DeletePlayerFromSector(dwSessionID, it->second->shX, it->second->shY);
	it->second->bDeleted = true;
	return true;
}

bool netPacketProc_MoveStart(DWORD dwsessionID, CPacket* packet)
{
	char csAction;
	short csX;
	short csY;

	*packet >> csAction;
	*packet >> csX;
	*packet >> csY;

	st_CHARACTER* _player = (*(m_CharacterMap.find(dwsessionID))).second;

	// 이건 나중에 로그로 넘기던가 해야함
	//char ipbuffer[50];
	//inet_ntop(AF_INET, &(session->IPPtr.sin_addr), ipbuffer, 50);
	//오차 범위 확인 -> 애초에 필요한가?
	//if (abs(_player->shX - csX) > dfERROR_RANGE || abs(_player->shY - csY) > dfERROR_RANGE)
	//{
	//	// 싱크 전송, 서버 주소로 수정
	//	printf("[SYNC - Packet MoveStart] Server x,y : %d, %d | Client x,y : %d, %d\n",
	//		_player->shX, _player->shY, csX, csY);
	//}

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
	CPacket csPacket(PROTOCOL_MAXSIZE);
	mpMoveStart(&header, &csPacket, _player->dwSessionID, _player->dwAction, csX, csY);
	
	SendPacket_Around(_player, &header, &csPacket);

	return true;
}

bool netPacketProc_MoveStop(DWORD dwsessionID, CPacket* packet)
{
	char csAction;
	short csX;
	short csY;

	*packet >> csAction;
	*packet >> csX;
	*packet >> csY;

	st_PACKET_HEADER header;
	CPacket scPacket(PROTOCOL_MAXSIZE);
	st_CHARACTER* _player = (*(m_CharacterMap.find(dwsessionID))).second;

	//오차 범위 확인
	if (abs(_player->shX - csX) > dfERROR_RANGE || abs(_player->shY - csY) > dfERROR_RANGE)
	{
		// 싱크 전송, 서버 주소로 수정
		printf("[SYNC - Packet MoveSTOP] ID : %d # Server x,y : %d, %d | Client x,y : %d, %d\n",
			dwsessionID, _player->shX, _player->shY, csX, csY);

		mpSync(&header, &scPacket, _player->dwSessionID, _player->shX, _player->shY);
		SendPacket_Around(_player, &header, &scPacket, true);
		scPacket.Clear();
	}
	else
	{
		_player->shX = csX;
		_player->shY = csY;
	}
	_player->dwAction = dfPACKET_MOVE_DIR_NONE;

	mpMoveStop(&header, &scPacket, _player->dwSessionID, _player->dwAction, csX, csY);
	SendPacket_Around(_player, &header, &scPacket);

	return true;
}

bool netPacketProc_Attack(DWORD dwsessionID, BYTE type)
{
	st_CHARACTER* _player = (*(m_CharacterMap.find(dwsessionID))).second;

	switch (type)
	{
	case dfPACKET_CS_ATTACK1:
		AttackProc(_player, type, dfATTACK1_RANGE_X, dfATTACK1_RANGE_Y, dfATTACK1_DAMAGE);
		break;
	case dfPACKET_CS_ATTACK2:
		AttackProc(_player, type, dfATTACK2_RANGE_X, dfATTACK2_RANGE_Y, dfATTACK2_DAMAGE);
		break;
	case dfPACKET_CS_ATTACK3:
		AttackProc(_player, type, dfATTACK3_RANGE_X, dfATTACK3_RANGE_Y, dfATTACK3_DAMAGE);
		break;
	}

	return true;
}

bool AttackProc(st_CHARACTER* player, BYTE type, BYTE xRange, BYTE yRange, char damage)
{
	// 싱크가 필요한지 모르겠음
	st_PACKET_HEADER header;
	CPacket scPacket = CPacket(PROTOCOL_MAXSIZE);
	switch (type)
	{
	case dfPACKET_CS_ATTACK1:
		mpAttack1(&header, &scPacket, player->dwSessionID, player->byDirection, player->shX, player->shY);
		break;
	case dfPACKET_CS_ATTACK2:
		mpAttack2(&header, &scPacket, player->dwSessionID, player->byDirection, player->shX, player->shY);
		break;
	case dfPACKET_CS_ATTACK3:
		mpAttack3(&header, &scPacket, player->dwSessionID, player->byDirection, player->shX, player->shY);
		break;
	}
	SendPacket_Around(player, &header, &scPacket);

	CStack<st_CHARACTER*> collideStack;
	CollisionCheck(player, player->byDirection, dfATTACK1_RANGE_X, dfATTACK1_RANGE_Y, &collideStack);

	while (!collideStack.empty())
	{
		scPacket.Clear();
		mpDamage(&header, &scPacket, player->dwSessionID, (collideStack.top())->dwSessionID, dfATTACK1_DAMAGE);
		collideStack.pop();

		// 공격자와 피격자의 섹터가 다르면 모든 주변 섹터에 보내야한다.
		// @@shs 일단 공격자 주변에만 보내게 하자.
		SendPacket_Around(player, &header, &scPacket);
	}

	return true;
}

bool netPacketProc_Echo(DWORD dwsessionID, CPacket* cPacket)
{
	st_PACKET_HEADER header;
	CPacket scPacket(PROTOCOL_MAXSIZE);

	DWORD _echoTime;
	(*cPacket) >> _echoTime;
	 
	mpEcho(&header, &scPacket, _echoTime);
	bool bRet = Send_UniCast(dwsessionID, &header, scPacket.GetBufferPtr());
	if (!bRet)
	{
		SetDeleteCharacter(dwsessionID);
		return false;
	}

	return true;
}

// pCenterPlayer를 중심으로 범위 내의 적을 pCheckedList에 넣는다.
void CollisionCheck(st_CHARACTER* pCenterPlayer, char chDir, BYTE xRange, BYTE yRange, CStack<st_CHARACTER*>* pCheckedStack)
{
	unordered_map<DWORD, st_CHARACTER*>::iterator it;

	for (it = m_CharacterMap.begin(); it != m_CharacterMap.end(); it++)
	{
		st_CHARACTER* _player = (*it).second;
		if (_player->bDeleted)
			continue;

		if (_player->dwSessionID == pCenterPlayer->dwSessionID)
			continue;

		switch (chDir)
		{
		case dfPACKET_MOVE_DIR_LL:
			if (_player->shX >= pCenterPlayer->shX - xRange && _player->shX <= pCenterPlayer->shX &&
				_player->shY >= pCenterPlayer->shY - yRange && _player->shY <= pCenterPlayer->shY + yRange)
			{
				pCheckedStack->push(_player);
			}
			break;
		case dfPACKET_MOVE_DIR_RR:
			if (_player->shX <= pCenterPlayer->shX + xRange && _player->shX >= pCenterPlayer->shX &&
				_player->shY >= pCenterPlayer->shY - yRange && _player->shY <= pCenterPlayer->shY + yRange)
			{
				pCheckedStack->push(_player);
			}
			break;
		}
	}
}

bool netPacketProc_Accept(DWORD dwsessionID)
{
	st_CHARACTER* playerPtr = _CharacterPool.Alloc();//(st_CHARACTER*)malloc(sizeof(st_CHARACTER));
	if (playerPtr == nullptr)
	{
		err_quit("accept_malloc");
		return false;
	}

	//playerPtr->pSession = session;
	playerPtr->bDeleted = false;
	playerPtr->dwSessionID = dwsessionID;
	playerPtr->byDirection = dfPACKET_MOVE_DIR_RR;
	playerPtr->dwAction = dfPACKET_MOVE_DIR_NONE;
	playerPtr->shX = dfRANGE_MOVE_LEFT + (rand() % (dfRANGE_MOVE_RIGHT - dfRANGE_MOVE_LEFT + 1));
	playerPtr->shY = dfRANGE_MOVE_TOP + (rand() % (dfRANGE_MOVE_BOTTOM - dfRANGE_MOVE_TOP + 1));
	playerPtr->chHP = dfHP_MAX;

	st_PACKET_HEADER header;
	CPacket scPacket(PROTOCOL_MAXSIZE);
	scPacket.Clear();
	
	mpCreateMyCharacter(&header, &scPacket, playerPtr->dwSessionID, playerPtr->byDirection, playerPtr->shX, playerPtr->shY, playerPtr->chHP);
	bool bSendRet = Send_UniCast(playerPtr->dwSessionID, &header, scPacket.GetBufferPtr());
	if (!bSendRet)
	{
		return false;
	}
	scPacket.Clear();

	SetUserToSector(playerPtr);

	mpCreateOtherCharacter(&header, &scPacket, playerPtr->dwSessionID, playerPtr->byDirection, playerPtr->shX, playerPtr->shY, playerPtr->chHP);
	SendPacket_Around(playerPtr, &header, &scPacket);
	scPacket.Clear();

	m_CharacterMap.insert({ playerPtr->dwSessionID, playerPtr });
	_LOG(0, L"Accepted Player # playerID : %d # playerX : %d # playerY : %d\n", playerPtr->dwSessionID, playerPtr->shX, playerPtr->shY);

	return true;
}


bool CharacterMoveCheck(short shX, short shY)
{
	if (shX < dfRANGE_MOVE_LEFT || shX >= dfRANGE_MOVE_RIGHT)
		return false;

	if (shY < dfRANGE_MOVE_TOP || shY >= dfRANGE_MOVE_BOTTOM)
		return false;

	return true;
}