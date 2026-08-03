#include <Windows.h>
#include <list>
#include <unordered_map>
using namespace std;

#include "FrameProc.h"
#include "TCPDefine.h"
#include "PacketDefine.h"
#include "TCPNetwork.h"
#include "SectorDefine.h"
#include "MessageProc.h"
#include "ContentsDefine.h"
#include "CStack.h"
#include "ContentsProc.h"
#include "SectorProc.h"
#include "MessageCreate.h"
#include "CFreeList.h"
#include "LogProc.h"
#include "ProcademyProfiler.h"

extern int _logicFrame;

extern int g_iLogLevel;
extern WCHAR g_szLogBuff[1024];

unordered_map<DWORD, st_CHARACTER*> m_CharacterMap;
procademy::CMemoryPool<st_CHARACTER> _CharacterPool(dfMAX_CONNECT, false, false);

DWORD dwCurrentTick;
DWORD dwNetworkCurrentTick;

void Update()
{
	//if (Skip())
		//return;

	//Profiler("Update");
	_logicFrame++;
	double oldTick = dwCurrentTick;
	dwCurrentTick = timeGetTime();
	//_LOG(0, L"Update!\n");

	DWORD dwDeltaTime = dwCurrentTick - oldTick;
	dwNetworkCurrentTick = timeGetTime();


	//double deltaRatio = ((double)dwDeltaTime) / (double)FRAME_TIME;
	//short shDeltaX = (short)(deltaRatio * dfSPEED_PLAYER_X);
	//short shDeltaY = (short)(deltaRatio * dfSPEED_PLAYER_Y);
	//_LOG(2, L"dwDelatTime : %d # shDeltaX : %d # shDeltaY : %d # temp : %f\n", dwDeltaTime, shDeltaX, shDeltaY, ((float)dwDeltaTime) / ((float)FRAME_TIME));

	st_CHARACTER* pPlayer = nullptr;
	unordered_map<DWORD, st_CHARACTER*>::iterator it;
	for (it = m_CharacterMap.begin(); it != m_CharacterMap.end();)
	{
		pPlayer = (*it).second;
		it++;

		if (pPlayer->bDeleted)
			continue;

		if (pPlayer->chHP <= 0)
		{
			pPlayer->bDeleted = true;
			DisconnectSession(pPlayer->pSession);
			continue;
		}

		if (timeGetTime() - (pPlayer->pSession->dwLastRecvTime) > dfNETWORK_PACKET_RECV_TIMEOUT)
		{
			// е╦юс╬ф©Т
			//_LOG(2, L"TimeOut Session # ID : %d\n", pPlayer->dwSessionID);
			pPlayer->bDeleted = true;
			DisconnectSession(pPlayer->pSession);
			continue;
		}

		switch (pPlayer->dwAction)
		{
		case dfPACKET_MOVE_DIR_LL:
			if (CharacterMoveCheck(pPlayer->shX - dfSPEED_PLAYER_X, pPlayer->shY))
			{
				pPlayer->shX -= dfSPEED_PLAYER_X;
			}
			break;
		case dfPACKET_MOVE_DIR_LU:
			if (CharacterMoveCheck(pPlayer->shX - dfSPEED_PLAYER_X, pPlayer->shY - dfSPEED_PLAYER_Y))
			{
				pPlayer->shX -= dfSPEED_PLAYER_X;
				pPlayer->shY -= dfSPEED_PLAYER_Y;
			}
			break;
		case dfPACKET_MOVE_DIR_UU:
			if (CharacterMoveCheck(pPlayer->shX, pPlayer->shY - dfSPEED_PLAYER_Y))
			{
				pPlayer->shY -= dfSPEED_PLAYER_Y;
			}
			break;
		case dfPACKET_MOVE_DIR_RU:
			if (CharacterMoveCheck(pPlayer->shX + dfSPEED_PLAYER_X, pPlayer->shY - dfSPEED_PLAYER_Y))
			{
				pPlayer->shX += dfSPEED_PLAYER_X;
				pPlayer->shY -= dfSPEED_PLAYER_Y;
			}
			break;
		case dfPACKET_MOVE_DIR_RR:
			if (CharacterMoveCheck(pPlayer->shX + dfSPEED_PLAYER_X, pPlayer->shY))
			{
				pPlayer->shX += dfSPEED_PLAYER_X;
			}
			break;
		case dfPACKET_MOVE_DIR_RD:
			if (CharacterMoveCheck(pPlayer->shX + dfSPEED_PLAYER_X, pPlayer->shY + dfSPEED_PLAYER_Y))
			{
				pPlayer->shX += dfSPEED_PLAYER_X;
				pPlayer->shY += dfSPEED_PLAYER_Y;
			}
			break;
		case dfPACKET_MOVE_DIR_DD:
			if (CharacterMoveCheck(pPlayer->shX, pPlayer->shY + dfSPEED_PLAYER_Y))
			{
				pPlayer->shY += dfSPEED_PLAYER_Y;
			}
			break;
		case dfPACKET_MOVE_DIR_LD:
			if (CharacterMoveCheck(pPlayer->shX - dfSPEED_PLAYER_X, pPlayer->shY + dfSPEED_PLAYER_Y))
			{
				pPlayer->shX -= dfSPEED_PLAYER_X;
				pPlayer->shY += dfSPEED_PLAYER_Y;
			}
			break;
		}

		{
			if (!UpdateSector(pPlayer))
			{
				ChangeSector(pPlayer);
			}
		}
	}
}

int GetCharacterCount()
{
	return m_CharacterMap.size();
}