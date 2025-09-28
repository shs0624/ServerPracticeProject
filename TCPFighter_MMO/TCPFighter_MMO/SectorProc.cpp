#include <list>
#include <vector>
#include <unordered_set>
using namespace std;

#include "TCPDefine.h"
#include "SectorDefine.h"
#include "ContentsDefine.h"
#include "PacketDefine.h"
#include "CStack.h"
#include "SectorProc.h"
#include "MessageCreate.h"
#include "TCPNetwork.h"
#include "ContentsProc.h"
#include "LogProc.h"
#include "ProcademyProfiler.h"

// 한 섹터는 100 x 100 크기로,  64 x 64개의 섹터로 이루어짐
vector<st_CHARACTER*> m_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

extern int g_iLogLevel;
extern WCHAR g_szLogBuff[1024];


// 유저를 섹터에 세팅하고, 그 섹터의 타 유저 정보도 전송
void SetUserToSector(st_CHARACTER* player)
{
	short sectorX = (player->shX) / dfSECTOR_SIZE_X;
	short sectorY = (player->shY) / dfSECTOR_SIZE_Y;

	player->CurSector.iX = sectorX;
	player->CurSector.iY = sectorY;

	player->OldSector.iX = sectorX;
	player->OldSector.iY = sectorY;

	// 주변 섹터의 유저 정보 새로운 유저에게 전달
	SendUserInfoToNewPlayer(player->pSession, sectorX, sectorY);

	m_Sector[sectorY][sectorX].push_back(player);
}

// 주변 섹터의 유저 정보 새로운 유저에게 전달
void SendUserInfoToNewPlayer(st_SESSION* pSession, short sectorX, short sectorY)
{
	static CStack<st_CHARACTER*> pSectorSessionStack;
	static CPacket scPacket(PROTOCOL_MAXSIZE);

	st_SECTOR_AROUND aroundSector;
	GetSectorAround(sectorX, sectorY, &aroundSector);

	for (int i = 0; i < aroundSector.iCount; i++)
	{
		short _tempSectorX = aroundSector.Around[i].iX;
		short _tempSectorY = aroundSector.Around[i].iY;

		GetSectorSessions(_tempSectorX, _tempSectorY, pSectorSessionStack);

		// 섹터의 유저들 생성 메세지를 pSession에 전송
		while (!pSectorSessionStack.empty())
		{
			scPacket.Clear();

			st_CHARACTER* pCharacter = pSectorSessionStack.top();
			pSectorSessionStack.pop();

			mpCreateOtherCharacter(&scPacket, pCharacter->dwSessionID, 
				pCharacter->byDirection, pCharacter->shX, pCharacter->shY, pCharacter->chHP);
			Send_UniCast(pSession, &scPacket);

			// 멈춰있지 않다면 이동 패킷도 보내기.
			if (pCharacter->dwAction != dfPACKET_MOVE_DIR_NONE)
			{
				scPacket.Clear();

				mpMoveStart(&scPacket, pCharacter->dwSessionID, pCharacter->dwAction, pCharacter->shX, pCharacter->shY);
				Send_UniCast(pSession, &scPacket);
			}
		}
	}
}

void DeletePlayerFromSector(st_CHARACTER* pPlayer)
{
	//Profiler("DeletePlayerFromSector");
	vector<st_CHARACTER*>& refSectorVector = m_Sector[pPlayer->CurSector.iY][pPlayer->CurSector.iX];
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		if (refSectorVector[i]->dwSessionID == pPlayer->dwSessionID)
		{
			refSectorVector.erase(refSectorVector.begin() + i);
			return;
		}
	}

	printf("DeletePlayerSector NotFound!\n");
	DebugBreak();
}

bool UpdateSector(st_CHARACTER* player)
{
	short sectorX = (player->shX) / dfSECTOR_SIZE_X;
	short sectorY = (player->shY) / dfSECTOR_SIZE_Y;

	short oldSectorX = player->OldSector.iX;
	short oldSectorY = player->OldSector.iY;

	if (sectorX != player->CurSector.iX || sectorY != player->CurSector.iY)
	{
		DeletePlayerFromSector(player);

		player->OldSector.iX = player->CurSector.iX;
		player->OldSector.iY = player->CurSector.iY;

		player->CurSector.iX = sectorX;
		player->CurSector.iY = sectorY;

		// 섹터에서 내 정보 이동
		m_Sector[player->CurSector.iY][player->CurSector.iX].push_back(player);

		_LOG(0, L"ID : %d # SectorChange # X :%d -> %d # Y : %d -> %d\n", player->dwSessionID, oldSectorX, sectorX, oldSectorY, sectorY);

		return false;
	}

	return true;
}

void ChangeSector(st_CHARACTER* player)
{
	//Profiler("ChangeSector");
	static CPacket scPacket(PROTOCOL_MAXSIZE);
	scPacket.Clear();

	st_SECTOR_AROUND removeSector;
	st_SECTOR_AROUND addSector;

	removeSector.iCount = 0;
	addSector.iCount = 0;

	// player의 OldSector, Cursector가 다른채로 있어야 한다.
	GetUpdateSectorAround(player, &removeSector, &addSector);
	_LOG(1, L"ChangeSector # playerID : %d # removeSectorCount : %d # addSectorCount : %d\n", player->dwSessionID, removeSector.iCount, addSector.iCount);

	scPacket.Clear();
	mpDeleteCharacter(&scPacket, player->dwSessionID);
	// RemoveSector 유저들에게 player 삭제 패킷 보내기.
	for (int i = 0; i < removeSector.iCount; i++)
	{
		int sectorX = removeSector.Around[i].iX;
		int sectorY = removeSector.Around[i].iY;

		SendPacket_SectorOne(sectorX, sectorY, &scPacket, NULL);
	}

	// player에게 RemoveSector 유저 삭제 패킷 보내기.
	for (int sectorIdx = 0; sectorIdx < removeSector.iCount; sectorIdx++)
	{
		int sectorX = removeSector.Around[sectorIdx].iX;
		int sectorY = removeSector.Around[sectorIdx].iY;

		vector<st_CHARACTER*>& refSectorVector = m_Sector[sectorY][sectorX];
		for (int sessionIdx = 0; sessionIdx < refSectorVector.size(); sessionIdx++)
		{
			scPacket.Clear();
			mpDeleteCharacter(&scPacket, refSectorVector[sessionIdx]->dwSessionID);
			Send_UniCast(player->pSession, &scPacket);
			//_LOG(0, L"Delete Character # ID : %d\n", refSectorVector[i]->dwSessionID);
		}
	}

	// player에게 Addsector 유저 정보 전송
	for (int sectorIdx = 0; sectorIdx < addSector.iCount; sectorIdx++)
	{
		// 그 섹터의 캐릭터들 생성 - player에게 전송
		vector<st_CHARACTER*>& refSectorVector = m_Sector[addSector.Around[sectorIdx].iY][addSector.Around[sectorIdx].iX];
		for (int sessionIdx = 0; sessionIdx < refSectorVector.size(); sessionIdx++)
		{
			if (refSectorVector[sessionIdx]->bDeleted)
				continue;

			scPacket.Clear();
			mpCreateOtherCharacter(&scPacket, refSectorVector[sessionIdx]->dwSessionID, refSectorVector[sessionIdx]->byDirection,
				refSectorVector[sessionIdx]->shX, refSectorVector[sessionIdx]->shY, refSectorVector[sessionIdx]->chHP);
			bool bRet = Send_UniCast(player->pSession, &scPacket);
			if (bRet == false)
				continue;

			if (refSectorVector[sessionIdx]->dwAction == dfPACKET_MOVE_DIR_NONE)
				continue;

			scPacket.Clear();
			mpMoveStart(&scPacket, refSectorVector[sessionIdx]->dwSessionID, refSectorVector[sessionIdx]->dwAction,
				refSectorVector[sessionIdx]->shX, refSectorVector[sessionIdx]->shY);
			Send_UniCast(player->pSession, &scPacket);
		}
	}

	// AddSector의 세션들에게 player 생성, 액션 전달
	for (int i = 0; i < addSector.iCount; i++)
	{
		int sectorX = addSector.Around[i].iX;
		int sectorY = addSector.Around[i].iY;

		scPacket.Clear();
		mpCreateOtherCharacter(&scPacket, player->dwSessionID, player->byDirection,
			player->shX, player->shY, player->chHP);
		SendPacket_SectorOne(sectorX, sectorY, &scPacket, player->dwSessionID);

		if (player->dwAction == dfPACKET_MOVE_DIR_NONE)
			continue;

		scPacket.Clear();
		mpMoveStart(&scPacket, player->dwSessionID, player->dwAction, player->shX, player->shY);
		SendPacket_SectorOne(sectorX, sectorY, &scPacket, player->dwSessionID);
	}
	//player->OldSector = player->CurSector;
	//_LOG(1, L"ChnageSector # playerID : %d # oldSector : %d, %d # curSector : %d,%d\n", player->dwSessionID, player->OldSector.iX, player->OldSector.iY,
	//	player->CurSector.iX, player->CurSector.iY);
}


void GetSectorSessions(short sectorX, short sectorY, CStack<st_CHARACTER*>& pPlayerStack)
{
	vector<st_CHARACTER*>& refSectorVector = m_Sector[sectorY][sectorX];
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		pPlayerStack.push(refSectorVector[i]);
	}
}

void GetAttackTargetSector(int playerX, int playerY, BYTE dir, BYTE xRange, BYTE yRange, st_SECTOR_AROUND* pSectorAttack)
{
	// static 지역컨테이너 unordered_map 선언
	static std::unordered_set<unsigned int> attackSectorSet;
	attackSectorSet.clear();

	short minX;
	short maxX;
	if (dir == dfPACKET_MOVE_DIR_LL)
	{
		minX = (playerX - xRange) >= dfRANGE_MOVE_LEFT ? (playerX - xRange) : dfRANGE_MOVE_LEFT;
		maxX = playerX;
	}
	else
	{
		minX = playerX;
		maxX = (playerX + xRange) < dfRANGE_MOVE_RIGHT ? (playerX + xRange) : dfRANGE_MOVE_RIGHT - 1;
	}

	short maxY = (playerY + yRange) < dfRANGE_MOVE_BOTTOM ? (playerY + yRange) : dfRANGE_MOVE_BOTTOM - 1;
	short minY = (playerY - yRange) >= 0 ? (playerY - yRange) : 0;

	short attackSectorXMin = minX / dfSECTOR_SIZE_X;
	short attackSectorXMax = maxX / dfSECTOR_SIZE_X;

	short attackSectorYMax = maxY / dfSECTOR_SIZE_Y;
	short attackSectorYMin = minY / dfSECTOR_SIZE_Y;

	attackSectorSet.insert((unsigned int)((attackSectorXMin << 8) | attackSectorYMin));
	attackSectorSet.insert((unsigned int)((attackSectorXMin << 8) | attackSectorYMax));
	attackSectorSet.insert((unsigned int)((attackSectorXMax << 8) | attackSectorYMin));
	attackSectorSet.insert((unsigned int)((attackSectorXMax << 8) | attackSectorYMax));

	// 겹치는 섹터는 사라짐
	int cnt = 0;
	pSectorAttack->iCount = 0;
	std::unordered_set<unsigned int>::iterator it;
	for (it = attackSectorSet.begin(); it != attackSectorSet.end(); it++)
	{
		short X = (short)(*it) >> 8;
		short Y = (*it) & 0x000000ff;
		//printf("AttackSector X : %d # Y : %d\n", X, Y);

		pSectorAttack->Around[pSectorAttack->iCount].iX = X;
		pSectorAttack->Around[pSectorAttack->iCount].iY = Y;
		pSectorAttack->iCount++;
		cnt++;
	}

	//printf("AttackSectorCheck : %d\n", cnt);
}


void GetDamageShowSector(int shX, int shY, st_SECTOR_AROUND* pSectorShowAttack)
{
	short sectorX = shX / dfSECTOR_SIZE_X;
	short sectorY = shY / dfSECTOR_SIZE_Y;

	short pivotX = shX % dfSECTOR_SIZE_X;
	short pivotY = shY % dfSECTOR_SIZE_Y;

	short centerX = dfSECTOR_SIZE_X / 2;
	short centerY = dfSECTOR_SIZE_Y / 2;

	short startX, endX;
	short startY, endY;
	if (pivotX >= centerX && pivotY >= centerY) 
	{
		// 우측 하단
		startX = 0;
		endX = 1;

		startY = 0;
		endY = 1;
	}
	else if (pivotX < centerX && pivotY >= centerY)
	{
		// 좌측하단
		startX = -1;
		endX = 0;

		startY = 0;
		endY = 1;
	}
	else if (pivotX >= centerX && pivotY < centerY)
	{
		// 우측상단
		startX = 0;
		endX = 1;

		startY = -1;
		endY = 0;
	}
	else if (pivotX < centerX && pivotY < centerY)
	{
		// 좌측상단
		startX = -1;
		endX = 0;

		startY = -1;
		endY = 0;
	}

	pSectorShowAttack->iCount = 0;
	for (int iY = startY; iY <= endY; iY++)
	{
		if (sectorY + iY < 0 || sectorY + iY >= dfSECTOR_MAX_Y)
			continue;

		for (int iX = startX; iX <= endX; iX++)
		{
			if (sectorX + iX < 0 || sectorX + iX >= dfSECTOR_MAX_X)
				continue;

			pSectorShowAttack->Around[pSectorShowAttack->iCount].iX = sectorX + iX;
			pSectorShowAttack->Around[pSectorShowAttack->iCount].iY = sectorY + iY;
			pSectorShowAttack->iCount++;
		}
	}
}

void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
{
	pSectorAround->iCount = 0;
	// 여기서 전부 밀어버려야되나?

	for (int iY = -1; iY <= 1; iY++)
	{
		if (iSectorY + iY < 0 || iSectorY + iY >= dfSECTOR_MAX_Y)
			continue;

		for (int iX = -1; iX <= 1; iX++)
		{
			if (iSectorX + iX < 0 || iSectorX + iX >= dfSECTOR_MAX_X)
				continue;

			pSectorAround->Around[pSectorAround->iCount].iX = iSectorX + iX;
			pSectorAround->Around[pSectorAround->iCount].iY = iSectorY + iY;
			pSectorAround->iCount++;
		}
	}
}

void GetUpdateSectorAround(st_CHARACTER* player, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector)
{
	int curSectorX = player->CurSector.iX;
	int curSectorY = player->CurSector.iY;

	int oldSectorX = player->OldSector.iX;
	int oldSectorY = player->OldSector.iY;

	// CurSector - OldSector로 이동 방향 얻어내기
	int moveX = curSectorX - oldSectorX;
	int moveY = curSectorY - oldSectorY;

	// 위로 이동
	if (moveX == 0 && moveY == -1)
	{
		// addSector
		int addXArr[3] = { -1, 0 ,1 };
		int addYArr[3] = { -1, -1 ,-1 };

		for (int i = 0; i < 3; i++)
		{
			int sectorX = curSectorX + addXArr[i];
			int sectorY = curSectorY + addYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pAddSector->Around[pAddSector->iCount].iX = sectorX;
			pAddSector->Around[pAddSector->iCount].iY = sectorY;
			pAddSector->iCount++;
		}

		// removeSector
		int removeXArr[3] = { -1, 0 ,1 };
		int removeYArr[3] = { 1, 1 ,1 };

		for (int i = 0; i < 3; i++)
		{
			int sectorX = oldSectorX + removeXArr[i];	
			int sectorY = oldSectorY + removeYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pRemoveSector->Around[pRemoveSector->iCount].iX = sectorX;
			pRemoveSector->Around[pRemoveSector->iCount].iY = sectorY;
			pRemoveSector->iCount++;
		}
	}
	// 우로 이동
	else if (moveX == 1 && moveY == 0)
	{
		// addSector
		int addXArr[3] = { 1, 1 ,1 };
		int addYArr[3] = { -1, 0 ,1 };

		for (int i = 0; i < 3; i++)
		{
			int sectorX = curSectorX + addXArr[i];
			int sectorY = curSectorY + addYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pAddSector->Around[pAddSector->iCount].iX = sectorX;
			pAddSector->Around[pAddSector->iCount].iY = sectorY;
			pAddSector->iCount++;
		}

		// removeSector
		int removeXArr[3] = { -1, -1 ,-1 };
		int removeYArr[3] = { -1, 0 ,1 };

		for (int i = 0; i < 3; i++)
		{
			int sectorX = oldSectorX + removeXArr[i];
			int sectorY = oldSectorY + removeYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pRemoveSector->Around[pRemoveSector->iCount].iX = sectorX;
			pRemoveSector->Around[pRemoveSector->iCount].iY = sectorY;
			pRemoveSector->iCount++;
		}
	}
	// 아래로 이동
	else if (moveX == 0 && moveY == 1)
	{
		// addSector
		int addXArr[3] = { -1, 0 ,1 };
		int addYArr[3] = { 1, 1 ,1 };

		for (int i = 0; i < 3; i++)
		{
			int sectorX = curSectorX + addXArr[i];
			int sectorY = curSectorY + addYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pAddSector->Around[pAddSector->iCount].iX = sectorX;
			pAddSector->Around[pAddSector->iCount].iY = sectorY;
			pAddSector->iCount++;
		}

		// removeSector
		int removeXArr[3] = { -1, 0 ,1 };
		int removeYArr[3] = { -1, -1 ,-1 };

		for (int i = 0; i < 3; i++)
		{
			int sectorX = oldSectorX + removeXArr[i];
			int sectorY = oldSectorY + removeYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pRemoveSector->Around[pRemoveSector->iCount].iX = sectorX;
			pRemoveSector->Around[pRemoveSector->iCount].iY = sectorY;
			pRemoveSector->iCount++;
		}
	}
	// 좌로 이동
	else if (moveX == -1 && moveY == 0)
	{
		// addSector
		int addXArr[3] = { -1, -1 ,-1 };
		int addYArr[3] = { -1, 0 ,1 };

		for (int i = 0; i < 3; i++)
		{
			int sectorX = curSectorX + addXArr[i];
			int sectorY = curSectorY + addYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pAddSector->Around[pAddSector->iCount].iX = sectorX;
			pAddSector->Around[pAddSector->iCount].iY = sectorY;
			pAddSector->iCount++;
		}

		// removeSector
		int removeXArr[3] = { 1, 1 ,1 };
		int removeYArr[3] = { -1, 0 ,1 };

		for (int i = 0; i < 3; i++)
		{
			int sectorX = oldSectorX + removeXArr[i];
			int sectorY = oldSectorY + removeYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pRemoveSector->Around[pRemoveSector->iCount].iX = sectorX;
			pRemoveSector->Around[pRemoveSector->iCount].iY = sectorY;
			pRemoveSector->iCount++;
		}
	}
	// 좌상단 이동
	else if (moveX == -1 && moveY == -1)
	{
		// addSector
		int addXArr[5] = { -1, -1 ,-1, 0, 1 };
		int addYArr[5] = { 1, 0 ,-1, -1, -1 };

		for (int i = 0; i < 5; i++)
		{
			int sectorX = curSectorX + addXArr[i];
			int sectorY = curSectorY + addYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pAddSector->Around[pAddSector->iCount].iX = sectorX;
			pAddSector->Around[pAddSector->iCount].iY = sectorY;
			pAddSector->iCount++;
		}

		// removeSector
		int removeXArr[5] = { -1, 0 ,1, 1, 1 };
		int removeYArr[5] = { 1, 1 , 1, 0, -1};

		for (int i = 0; i < 5; i++)
		{
			int sectorX = oldSectorX + removeXArr[i];
			int sectorY = oldSectorY + removeYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pRemoveSector->Around[pRemoveSector->iCount].iX = sectorX;
			pRemoveSector->Around[pRemoveSector->iCount].iY = sectorY;
			pRemoveSector->iCount++;
		}
	}
	// 우상단 이동
	else if (moveX == 1 && moveY == -1)
	{
		// addSector
		int addXArr[5] = { -1, 0 ,1, 1, 1 };
		int addYArr[5] = { -1, -1 ,-1, 0, 1 };

		for (int i = 0; i < 5; i++)
		{
			int sectorX = curSectorX + addXArr[i];
			int sectorY = curSectorY + addYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pAddSector->Around[pAddSector->iCount].iX = sectorX;
			pAddSector->Around[pAddSector->iCount].iY = sectorY;
			pAddSector->iCount++;
		}

		// removeSector
		int removeXArr[5] = { -1, -1 ,-1, 0, 1 };
		int removeYArr[5] = { -1, 0 , 1, 1, 1 };

		for (int i = 0; i < 5; i++)
		{
			int sectorX = oldSectorX + removeXArr[i];
			int sectorY = oldSectorY + removeYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pRemoveSector->Around[pRemoveSector->iCount].iX = sectorX;
			pRemoveSector->Around[pRemoveSector->iCount].iY = sectorY;
			pRemoveSector->iCount++;
		}
	}
	// 우하단 이동
	else if (moveX == 1 && moveY == 1)
	{
		// addSector
		int addXArr[5] = { -1, 0 ,1, 1, 1 };
		int addYArr[5] = { 1, 1 ,1, 0, -1 };

		for (int i = 0; i < 5; i++)
		{
			int sectorX = curSectorX + addXArr[i];
			int sectorY = curSectorY + addYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pAddSector->Around[pAddSector->iCount].iX = sectorX;
			pAddSector->Around[pAddSector->iCount].iY = sectorY;
			pAddSector->iCount++;
		}

		// removeSector
		int removeXArr[5] = { -1, -1 ,-1, 0, 1 };
		int removeYArr[5] = { 1, 0 , -1, -1, -1 };

		for (int i = 0; i < 5; i++)
		{
			int sectorX = oldSectorX + removeXArr[i];
			int sectorY = oldSectorY + removeYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pRemoveSector->Around[pRemoveSector->iCount].iX = sectorX;
			pRemoveSector->Around[pRemoveSector->iCount].iY = sectorY;
			pRemoveSector->iCount++;
		}
	}
	// 좌하단 이동
	else if (moveX == -1 && moveY == 1)
	{
		// addSector
		int addXArr[5] = { -1, -1 ,-1, 0, 1 };
		int addYArr[5] = { -1, 0 ,1, 1, 1 };

		for (int i = 0; i < 5; i++)
		{
			int sectorX = curSectorX + addXArr[i];
			int sectorY = curSectorY + addYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pAddSector->Around[pAddSector->iCount].iX = sectorX;
			pAddSector->Around[pAddSector->iCount].iY = sectorY;
			pAddSector->iCount++;
		}

		// removeSector
		int removeXArr[5] = { -1, 0 ,1, 1, 1 };
		int removeYArr[5] = { -1, -1 , -1, 0, 1 };

		for (int i = 0; i < 5; i++)
		{
			int sectorX = oldSectorX + removeXArr[i];
			int sectorY = oldSectorY + removeYArr[i];

			if (sectorX < 0 || sectorX >= dfSECTOR_MAX_X || sectorY < 0 || sectorY >= dfSECTOR_MAX_Y)
				continue;

			pRemoveSector->Around[pRemoveSector->iCount].iX = sectorX;
			pRemoveSector->Around[pRemoveSector->iCount].iY = sectorY;
			pRemoveSector->iCount++;
		}
	}
}


//void GetUpdateSectorAround(st_CHARACTER* player, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector)
//{
//	// 1. OldSector, CurSector의 Around 구하기
//	st_SECTOR_AROUND oldAround;
//	st_SECTOR_AROUND curAround;
//
//	GetSectorAround(player->OldSector.iX, player->OldSector.iY, &oldAround);
//	GetSectorAround(player->CurSector.iX, player->CurSector.iY, &curAround);
//
//	// 2. OldSector엔 있지만 CurSector엔 없는 섹터는 RemoveSector에 추가
//	for (int oldIdx = 0; oldIdx < oldAround.iCount; oldIdx++)
//	{
//		bool bFlag = false;
//		int iOldSectorX = oldAround.Around[oldIdx].iX;
//		int iOldSectorY = oldAround.Around[oldIdx].iY;
//
//		for (int curIdx = 0; curIdx < curAround.iCount; curIdx++)
//		{
//			int iCurSectorX = curAround.Around[curIdx].iX;
//			int iCurSectorY = curAround.Around[curIdx].iY;
//
//			// CurSector에 있다
//			if (iCurSectorX == iOldSectorX && iCurSectorY == iOldSectorY)
//			{
//				bFlag = true;
//				break;
//			}
//		}
//
//		// OldSector엔 있지만 CurSector에 없으니까 RemoveSector다.
//		if (!bFlag)
//		{
//			pRemoveSector->Around[pRemoveSector->iCount].iX = iOldSectorX;
//			pRemoveSector->Around[pRemoveSector->iCount].iY = iOldSectorY;
//			pRemoveSector->iCount++;
//		}
//	}
//
//	// 3. OldSector엔 없지만 CurSector엔 있는 섹터는 AddSector에 추가
//	for (int curIdx = 0; curIdx < curAround.iCount; curIdx++)
//	{
//		bool bFlag = false;
//		int iCurSectorX = curAround.Around[curIdx].iX;
//		int iCurSectorY = curAround.Around[curIdx].iY;
//
//		for (int oldIdx = 0; oldIdx < oldAround.iCount; oldIdx++)
//		{
//			int iOldSectorX = oldAround.Around[oldIdx].iX;
//			int iOldSectorY = oldAround.Around[oldIdx].iY;
//
//			// OldSector에 있다
//			if (iCurSectorX == iOldSectorX && iCurSectorY == iOldSectorY)
//			{
//				bFlag = true;
//				break;
//			}
//		}
//
//		// CurSector엔 있지만 OldSector에 없으니까 AddSector다.
//		if (!bFlag)
//		{
//			pAddSector->Around[pAddSector->iCount].iX = iCurSectorX;
//			pAddSector->Around[pAddSector->iCount].iY = iCurSectorY;
//			pAddSector->iCount++;
//		}
//	}
//}

void SendPacket_SectorOne(int iSectorX, int iSectorY, CPacket* cPacket, DWORD dwExceptSessionID)
{
	//Profiler("SendPacket_SectorOne");
	vector<st_CHARACTER*>& refSectorVector = m_Sector[iSectorY][iSectorX];
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		if (refSectorVector[i]->dwSessionID == dwExceptSessionID)
			continue;

		Send_UniCast(refSectorVector[i]->pSession, cPacket);
	}
}

void SendPacket_Around(st_CHARACTER* pCharacter, CPacket* cPacket, bool bSendMe)
{
	//Profiler("SendPacket_Around");
	//int iSectorX = pCharacter->shX / dfSECTOR_SIZE_X;
	//int iSectorY = pCharacter->shY / dfSECTOR_SIZE_Y;
	int iSectorX = pCharacter->CurSector.iX;
	int iSectorY = pCharacter->CurSector.iY;
	if (iSectorX != pCharacter->CurSector.iX || iSectorY != pCharacter->CurSector.iY)
		_LOG(2, L"SectorXY Not equal Cursector! Calc X,Y : %d, %d # CurSector X,Y : %d, %d # X, Y : %d, %d\n", 
			iSectorX, iSectorY, pCharacter->CurSector.iX, pCharacter->CurSector.iY, pCharacter->shX, pCharacter->shY);

	st_SECTOR_AROUND stAround;
	GetSectorAround(iSectorX, iSectorY, &stAround);

	for (int i = 0; i < stAround.iCount; i++)
	{
		bool playerSectorFlag = false;
		bool bSendFlag = false;
		if (stAround.Around[i].iY == iSectorY && stAround.Around[i].iX == iSectorX)
			playerSectorFlag = true;

		vector<st_CHARACTER*>& refSectorVector = m_Sector[stAround.Around[i].iY][stAround.Around[i].iX];
		if (playerSectorFlag)
		{
			for (int j = 0; j < refSectorVector.size(); j++)
			{
				if (!bSendMe && refSectorVector[j]->dwSessionID == pCharacter->dwSessionID)
					continue;

				Send_UniCast(refSectorVector[j]->pSession, cPacket);
			}
		}
		else
		{
			for (int j = 0; j < refSectorVector.size(); j++)
			{
				Send_UniCast(refSectorVector[j]->pSession, cPacket);
			}
		}
	}
}