#include <WS2tcpip.h>
#include <Windows.h>
#include <list>
#include <vector>
#include <queue>
#include <unordered_map>
using namespace std;

#include "PacketDefine.h"
#include "TCPDefine.h"
#include "TCPNetwork.h"
#include "MessageProc.h"
#include "SectorDefine.h"
#include "ContentsDefine.h"
#include "CStack.h"
#include "SectorProc.h"
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

// 캐릭터 삭제, 메세지도 전송
void SetDeleteCharacter(DWORD dwSessionID)
{
	static CPacket csPacket(PROTOCOL_MAXSIZE);
	csPacket.Clear();

	unordered_map<DWORD, st_CHARACTER*>::iterator it;
	it = (m_CharacterMap.find(dwSessionID));
	if (it == m_CharacterMap.end())
		return;

	st_CHARACTER* ptr = it->second;

	mpDeleteCharacter(&csPacket, dwSessionID);

	SendPacket_Around(ptr, &csPacket);

	DeletePlayerFromSector(ptr);

	m_CharacterMap.erase(dwSessionID);

	_CharacterPool.Free(ptr);
	return;
}

bool netPacketProc_MoveStart(st_SESSION* pSession, CPacket* packet)
{
	static CPacket scPacket(PROTOCOL_MAXSIZE);
	scPacket.Clear();

	char csAction;
	short csX;
	short csY;

	*packet >> csAction;
	*packet >> csX;
	*packet >> csY;

	unordered_map<DWORD, st_CHARACTER*>::iterator it;
	it = (m_CharacterMap.find(pSession->dwSessionID));
	if (it == m_CharacterMap.end())
		return true;

	st_CHARACTER* _player = (*it).second;
	// 이건 나중에 로그로 넘기던가 해야함
	//char ipbuffer[50];
	//inet_ntop(AF_INET, &(session->IPPtr.sin_addr), ipbuffer, 50);
	//오차 범위 확인 -> 애초에 필요한가?
	//오차 범위 확인
	if (abs(_player->shX - csX) > dfERROR_RANGE || abs(_player->shY - csY) > dfERROR_RANGE)
	{
		// 싱크 전송, 서버 주소로 수정
		mpSync(&scPacket, _player->dwSessionID, _player->shX, _player->shY);
		SendPacket_Around(_player, &scPacket, true);
		scPacket.Clear();
	}
	else
	{
		_player->shX = csX;
		_player->shY = csY;

		if (!CharacterMoveCheck(_player->shX, _player->shY))
			DebugBreak();

		_LOG(0, L"MoveStart # playerID : %d # playerX : %d # playerY : %d\n", _player->dwSessionID, _player->shX, _player->shY);
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

	// 이동으로 섹터가 바뀌었다면, 섹터 변경
	if (!UpdateSector(_player))
	{
		_LOG(0, L"MoveStart UpdteSector # playerID : %d # playerX : %d # playerY : %d\n", _player->dwSessionID, _player->shX, _player->shY);
		ChangeSector(_player);
	}

	scPacket.Clear();
	mpMoveStart(&scPacket, _player->dwSessionID, _player->dwAction, _player->shX, _player->shY);
	SendPacket_Around(_player, &scPacket);

	return true;
}

bool netPacketProc_MoveStop(st_SESSION* pSession, CPacket* packet)
{
	static CPacket scPacket(PROTOCOL_MAXSIZE);
	scPacket.Clear();

	char csAction;
	short csX;
	short csY;

	*packet >> csAction;
	*packet >> csX;
	*packet >> csY;

	unordered_map<DWORD, st_CHARACTER*>::iterator it;
	it = (m_CharacterMap.find(pSession->dwSessionID));
	if (it == m_CharacterMap.end())
		return true;

	st_CHARACTER* _player = (*it).second;


	//오차 범위 확인
	if (abs(_player->shX - csX) > dfERROR_RANGE || abs(_player->shY - csY) > dfERROR_RANGE)
	{
		// 싱크 전송, 서버 주소로 수정
		mpSync(&scPacket, _player->dwSessionID, _player->shX, _player->shY);
		SendPacket_Around(_player, &scPacket, true);
		scPacket.Clear();
	}
	else
	{
		_player->shX = csX;
		_player->shY = csY;

		if (!CharacterMoveCheck(_player->shX, _player->shY))
			DebugBreak();

		_LOG(0, L"MoveStart # playerID : %d # playerX : %d # playerY : %d\n", _player->dwSessionID, _player->shX, _player->shY);
	}

	_player->byDirection = csAction;
	_player->dwAction = dfPACKET_MOVE_DIR_NONE;

	// 이동으로 섹터가 바뀌었다면, 섹터 변경
	if (!UpdateSector(_player))
	{
		_LOG(0, L"MoveStop UpdteSector # playerID : %d # playerX : %d # playerY : %d\n", _player->dwSessionID, _player->shX, _player->shY);
		ChangeSector(_player);
	}

	mpMoveStop(&scPacket, _player->dwSessionID, _player->byDirection, _player->shX, _player->shY);
	SendPacket_Around(_player, &scPacket);

	return true;
}

bool netPacketProc_Attack(st_SESSION* pSession, BYTE type, CPacket* packet)
{
	static CPacket scPacket(PROTOCOL_MAXSIZE);
	scPacket.Clear();

	char csDir;
	short csX;
	short csY;

	*packet >> csDir;
	*packet >> csX;
	*packet >> csY;

	unordered_map<DWORD, st_CHARACTER*>::iterator it;
	it = (m_CharacterMap.find(pSession->dwSessionID));
	if (it == m_CharacterMap.end())
		return true;

	st_CHARACTER* _player = (*it).second;

	//오차 범위 확인
	if (abs(_player->shX - csX) > dfERROR_RANGE || abs(_player->shY - csY) > dfERROR_RANGE)
	{
		// 싱크 전송, 서버 주소로 수정
		mpSync(&scPacket, _player->dwSessionID, _player->shX, _player->shY);
		SendPacket_Around(_player,&scPacket, true);
		scPacket.Clear();
	}
	else
	{
		_player->shX = csX;
		_player->shY = csY;

		if (!CharacterMoveCheck(_player->shX, _player->shY))
			DebugBreak();

		_LOG(0, L"MoveStart # playerID : %d # playerX : %d # playerY : %d\n", _player->dwSessionID, _player->shX, _player->shY);
	}

	// 이동으로 섹터가 바뀌었다면, 섹터 변경
	if (!UpdateSector(_player))
	{
		_LOG(0, L"Attack UpdteSector # playerID : %d # playerX : %d # playerY : %d\n", _player->dwSessionID, _player->shX, _player->shY);
		ChangeSector(_player);
	}

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
	//Profiler("AttackProc");
	// 싱크가 필요한지 모르겠음
	static CPacket scPacket(PROTOCOL_MAXSIZE);
	scPacket.Clear();

	switch (type)
	{
	case dfPACKET_CS_ATTACK1:
		mpAttack1(&scPacket, player->dwSessionID, player->byDirection, player->shX, player->shY);
		break;
	case dfPACKET_CS_ATTACK2:
		mpAttack2(&scPacket, player->dwSessionID, player->byDirection, player->shX, player->shY);
		break;
	case dfPACKET_CS_ATTACK3:
		mpAttack3(&scPacket, player->dwSessionID, player->byDirection, player->shX, player->shY);
		break;
	}
	SendPacket_Around(player, &scPacket);

	CollisionCheck(player, player->byDirection, xRange, yRange, damage);

	return true;
}

bool netPacketProc_Echo(st_SESSION* pSession, CPacket* cPacket)
{
	static CPacket scPacket(PROTOCOL_MAXSIZE);
	scPacket.Clear();

	DWORD _echoTime;
	(*cPacket) >> _echoTime;
	 
	mpEcho(&scPacket, _echoTime);
	bool bRet = Send_UniCast(pSession, &scPacket);
	if (!bRet)
	{
		DisconnectSession(pSession);
		return false;
	}

	return true;
}

// pCenterPlayer를 중심으로 범위 내의 적을 pCheckedList에 넣는다.
void CollisionCheck(st_CHARACTER* pCenterPlayer, char chDir, BYTE xRange, BYTE yRange, char chDamage)
{
	static CStack<st_CHARACTER*> pTargetStack;
	static CStack<st_CHARACTER*> pCollideCheckedStack(1000);
	static CPacket scPacket(PROTOCOL_MAXSIZE);

	pTargetStack.clear();
	pCollideCheckedStack.clear();

	int playerX = pCenterPlayer->shX;
	int playerY = pCenterPlayer->shY;

	st_SECTOR_AROUND attackTargetSector;
	GetAttackTargetSector(playerX, playerY, chDir, xRange, yRange, &attackTargetSector);

	for (int i = 0; i < attackTargetSector.iCount; i++)
	{
		int iX = attackTargetSector.Around[i].iX;
		int iY = attackTargetSector.Around[i].iY;

		GetSectorSessions(iX, iY, pTargetStack);
	}
	
	while (!pTargetStack.empty())
	{
		st_CHARACTER* pCharacter = pTargetStack.top();
		pTargetStack.pop();

		if (pCharacter->bDeleted)
			continue;

		if (pCharacter->dwSessionID == pCenterPlayer->dwSessionID)
			continue;

		switch (chDir)
		{
		case dfPACKET_MOVE_DIR_LL:
			if (pCharacter->shX >= pCenterPlayer->shX - xRange && pCharacter->shX <= pCenterPlayer->shX &&
				pCharacter->shY >= pCenterPlayer->shY - yRange && pCharacter->shY <= pCenterPlayer->shY + yRange)
			{
				pCharacter->chHP -= chDamage;
				if (pCharacter->chHP <= 0)
				{
					//SetDeleteCharacter_Direct(pCharacter);
					continue;
				}

				pCollideCheckedStack.push(pCharacter);
			}
			break;
		case dfPACKET_MOVE_DIR_RR:
			if (pCharacter->shX <= pCenterPlayer->shX + xRange && pCharacter->shX >= pCenterPlayer->shX &&
				pCharacter->shY >= pCenterPlayer->shY - yRange && pCharacter->shY <= pCenterPlayer->shY + yRange)
			{
				pCharacter->chHP -= chDamage;
				if (pCharacter->chHP <= 0)
				{
					//SetDeleteCharacter_Direct(pCharacter);
					continue;
				}

				pCollideCheckedStack.push(pCharacter);
			}
			break;
		}
	}
	
	// 데미지는 피격자 주변에 전송
	while (!pCollideCheckedStack.empty())
	{
		st_CHARACTER* ptr = pCollideCheckedStack.top();
		pCollideCheckedStack.pop();

		scPacket.Clear();
		mpDamage(&scPacket, pCenterPlayer->dwSessionID, ptr->dwSessionID, ptr->chHP);
		SendPacket_Around(ptr, &scPacket);
	}
}

bool netPacketProc_Accept(st_SESSION* session)
{
	static CPacket scPacket(PROTOCOL_MAXSIZE);
	st_CHARACTER* playerPtr = _CharacterPool.Alloc();
	if (playerPtr == nullptr)
	{
		err_quit("accept_malloc");
		return false;
	}

	playerPtr->pSession = session;
	playerPtr->bDeleted = false;
	playerPtr->dwSessionID = session->dwSessionID;
	playerPtr->byDirection = dfPACKET_MOVE_DIR_RR;
	playerPtr->dwAction = dfPACKET_MOVE_DIR_NONE;
	playerPtr->shX = rand() % dfRANGE_MOVE_RIGHT;
	playerPtr->shY = rand() % dfRANGE_MOVE_BOTTOM;
	playerPtr->chHP = dfHP_MAX;

	scPacket.Clear();
	mpCreateMyCharacter(&scPacket, playerPtr->dwSessionID, playerPtr->byDirection, playerPtr->shX, playerPtr->shY, playerPtr->chHP);
	bool bSendRet = Send_UniCast(session, &scPacket);
	if (!bSendRet)
	{
		return false;
	}
	scPacket.Clear();

	SetUserToSector(playerPtr);

	mpCreateOtherCharacter(&scPacket, playerPtr->dwSessionID, playerPtr->byDirection, playerPtr->shX, playerPtr->shY, playerPtr->chHP);
	SendPacket_Around(playerPtr, &scPacket);
	scPacket.Clear();

	m_CharacterMap.insert({ playerPtr->dwSessionID, playerPtr });

	if (!CharacterMoveCheck(playerPtr->shX, playerPtr->shY))
		DebugBreak();

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