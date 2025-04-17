#include <Windows.h>
#include "PacketDefine.h"
#include "TCPDefine.h"
#include "MessageProc.h"
#include "SectorDefine.h"
#include "ContentsDefine.h"
#include "SectorProc.h"
#include "ContentsProc.h"
#include "Debug.h"
#include <unordered_map>
using namespace std;

extern unordered_map<DWORD, st_CHARACTER*> m_CharacterMap;

bool netPacketProc_MoveStart(st_SESSION* session, CPacket* packet)
{

	return true;
}

bool netPacketProc_MoveStop(st_SESSION* session, CPacket* packet)
{

	return true;
}

bool netPacketProc_Attack1(st_SESSION* session)
{

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

void netPacket_CollisionCheck(st_SESSION* attacker, BYTE xRange, BYTE yRange, char damage)
{

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