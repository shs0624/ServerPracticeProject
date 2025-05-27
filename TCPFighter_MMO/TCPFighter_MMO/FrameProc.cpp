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
#include "ContentsProc.h"
#include "SectorProc.h"
#include "MessageCreate.h"
#include "LogProc.h"

extern int g_iLogLevel;
extern WCHAR g_szLogBuff[1024];

unordered_map<DWORD, st_CHARACTER*> m_CharacterMap;

DWORD dwCurrentTick;

void DisconnectPlayer();

void Update()
{
	if (Skip())
		return;

	DWORD oldTick = dwCurrentTick;
	dwCurrentTick = timeGetTime();
	//_LOG(0, L"Update!\n");

	DWORD dwDeltaTime = dwCurrentTick - oldTick;
	short shDeltaX = (short)(((float)(dwDeltaTime / FRAME_TIME)) * dfSPEED_PLAYER_X);
	short shDeltaY = (short)(((float)(dwDeltaTime / FRAME_TIME)) * dfSPEED_PLAYER_Y);

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
			DisconnectSession(pPlayer->pSession->Socket);
			pPlayer->bDeleted = true;
			continue;
		}
		
		if (dwCurrentTick - pPlayer->pSession->dwLastRecvTime > dfNETWORK_PACKET_RECV_TIMEOUT)
		{
			// 타임아웃
			DisconnectSession(pPlayer->pSession->Socket);
			pPlayer->bDeleted = true;
			continue;
		}

		_LOG(0, L"Player Info # sessionID : %d # X : %d # Y : %d\n", pPlayer->dwSessionID, pPlayer->shX, pPlayer->shY);

		switch (pPlayer->dwAction)
		{
		case dfPACKET_MOVE_DIR_LL:
			if (CharacterMoveCheck(pPlayer->shX - shDeltaX, pPlayer->shY))
			{
				pPlayer->shX -= shDeltaX;
			}
			break;
		case dfPACKET_MOVE_DIR_LD:
			if (CharacterMoveCheck(pPlayer->shX - shDeltaX, pPlayer->shY + shDeltaY))
			{
				pPlayer->shX -= shDeltaX;
				pPlayer->shY += shDeltaY;
			}
			break;
		case dfPACKET_MOVE_DIR_DD:
			if (CharacterMoveCheck(pPlayer->shX, pPlayer->shY + shDeltaY))
			{
				pPlayer->shY += shDeltaY;
			}
			break;
		case dfPACKET_MOVE_DIR_RD:
			if (CharacterMoveCheck(pPlayer->shX + shDeltaX, pPlayer->shY + shDeltaY))
			{
				pPlayer->shX += shDeltaX;
				pPlayer->shY += shDeltaY;
			}
			break;
		case dfPACKET_MOVE_DIR_RR:
			if (CharacterMoveCheck(pPlayer->shX + shDeltaX, pPlayer->shY))
			{
				pPlayer->shX += shDeltaX;
			}
			break;
		case dfPACKET_MOVE_DIR_RU:
			if (CharacterMoveCheck(pPlayer->shX + shDeltaX, pPlayer->shY - shDeltaY))
			{
				pPlayer->shX += shDeltaX;
				pPlayer->shY -= shDeltaY;
			}
			break;
		case dfPACKET_MOVE_DIR_UU:
			if (CharacterMoveCheck(pPlayer->shX, pPlayer->shY - shDeltaY))
			{
				pPlayer->shY -= shDeltaY;
			}
			break;
		case dfPACKET_MOVE_DIR_LU:
			if (CharacterMoveCheck(pPlayer->shX - shDeltaX, pPlayer->shY - shDeltaY))
			{
				pPlayer->shX -= shDeltaX;
				pPlayer->shY -= shDeltaY;
			}
			break;
		}

		if (!UpdateSector(pPlayer))
		{
			ChangeSector(pPlayer);
		}
	}

	DisconnectPlayer();
}

// 표시된 플레이어 map에서 삭제
void DisconnectPlayer()
{
	unordered_map<DWORD, st_CHARACTER*>::iterator it;
	CPacket csPacket(PROTOCOL_MAXSIZE);
	for (it = m_CharacterMap.begin(); it != m_CharacterMap.end();)
	{
		if ((*it).second->bDeleted)
		{
			//그 섹터의 플레이어에게 DeleteCharacter 전송
			st_PACKET_HEADER header;
			mpDeleteCharacter(&header, &csPacket, (*it).first);

			SendPacket_Around((*it).second, &header, &csPacket);
			csPacket.Clear();
			
			it = m_CharacterMap.erase(it);
			_LOG(0, L"Disconnect Player L7 # sessionID : %d\n", (*it).second->dwSessionID);
			continue;
		}

		it++;
	}
}

bool Skip()
{
	static DWORD _Tick = timeGetTime();
	static bool skipped = false;

	if (false == skipped)
	{
		DWORD t = timeGetTime() - _Tick;

		// 작아야 슬립하는거지. 소요 시간이 긴데, 슬립을 하면 안됐다.
		// 부호가 반대였는데, 그래서 이걸 활성화하면 프레임이 반토막 났던 것
		if (t < FRAME_TIME)
		{
			DWORD sleepTime = FRAME_TIME - t;
			Sleep(sleepTime);
		}
	}

	if (timeGetTime() - _Tick > (FRAME_TIME * 2))
	{
		return true;
	}

	_Tick += FRAME_TIME;

	return false;
}