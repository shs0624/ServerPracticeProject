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
#include "LogProc.h"
#include "ProcademyProfiler.h"

// 한 섹터는 100 x 100 크기로,  64 x 64개의 섹터로 이루어짐
//list<st_CHARACTER*> m_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
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

	//SendUserInfoToNewPlayer(player->dwSessionID, player->shX, player->shY);

	m_Sector[sectorY][sectorX].push_back(player);
}

void SendUserInfoToNewPlayer(DWORD dwNewSessionID, short shX, short shY)
{
	Profiler("SendUserInfoToNewPlayer");
	short sectorX = shX / dfSECTOR_SIZE_X;
	short sectorY = shY / dfSECTOR_SIZE_Y;

	st_PACKET_HEADER header;
	CPacket* scPacket = new CPacket(PROTOCOL_MAXSIZE);

	// 그 섹터의 유저들 정보를 새 유저에게 전송
	st_SECTOR_AROUND aroundSector;
	GetSectorAround(sectorX, sectorY, &aroundSector);

	for (int i = 0; i < aroundSector.iCount; i++)
	{
		int iX = aroundSector.Around[i].iX;
		int iY = aroundSector.Around[i].iY;

		vector<st_CHARACTER*>& refSectorVector = m_Sector[iY][iX];
		for (int i = 0; i < refSectorVector.size(); i++)
		{
			st_CHARACTER* ptrChar = refSectorVector[i];
			mpCreateOtherCharacter(&header, scPacket, ptrChar->dwSessionID, ptrChar->byDirection, ptrChar->shX, ptrChar->shY, ptrChar->chHP);
			bool bRet = Send_UniCast(dwNewSessionID, &header, scPacket->GetBufferPtr());
			if (!bRet)
			{
				return;
			}

			scPacket->Clear();

			// 그 유저가 이동중이라면 MOVESTART도 전송
			if (ptrChar->dwAction != dfPACKET_MOVE_DIR_NONE)
			{
				//scMoveStart
				mpMoveStart(&header, scPacket, ptrChar->dwSessionID, ptrChar->dwAction, ptrChar->shX, ptrChar->shY);
				bool bRet = Send_UniCast(dwNewSessionID, &header, scPacket->GetBufferPtr());
				if (!bRet)
				{
					return;
				}

				scPacket->Clear();
			}
		}
	}
}

void DeletePlayerFromSector(DWORD dwSessionID, short shX, short shY)
{
	Profiler("DeletePlayerFromSector");
	short sectorX = shX / dfSECTOR_SIZE_X;
	short sectorY = shY / dfSECTOR_SIZE_Y;

	vector<st_CHARACTER*>& refSectorVector = m_Sector[sectorY][sectorX];
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		if (refSectorVector[i]->dwSessionID == dwSessionID)
		{
			refSectorVector.erase(refSectorVector.begin() + i);
			return;
		}
	}
}

bool UpdateSector(st_CHARACTER* player)
{
	short sectorX = (player->shX) / dfSECTOR_SIZE_X;
	short sectorY = (player->shY) / dfSECTOR_SIZE_Y;

	player->CurSector.iX = sectorX;
	player->CurSector.iY = sectorY;

	if (player->OldSector.iX != player->CurSector.iX || player->OldSector.iY != player->CurSector.iY)
	{
		return false;
	}

	return true;
}

void ChangeSector(st_CHARACTER* player)
{
	Profiler("ChangeSector");
	st_SECTOR_AROUND removeSector;
	st_SECTOR_AROUND addSector;

	removeSector.iCount = 0;
	addSector.iCount = 0;

	// player의 OldSector, Cursector가 다른채로 있어야 한다.
	GetUpdateSectorAround(player, &removeSector, &addSector);

	st_PACKET_HEADER header;
	CPacket scPacket(PROTOCOL_MAXSIZE);

	// 1. RemoveSector의 캐릭터 Delete 메세지 -> 탐색중인 player에게 send
	for (int i = 0; i < removeSector.iCount; i++)
	{
		vector<st_CHARACTER*>& refSectorVector = m_Sector[removeSector.Around[i].iY][removeSector.Around[i].iX];
		for (int i = 0; i < refSectorVector.size(); i++)
		{
			if (refSectorVector[i]->bDeleted)
				continue;

			mpDeleteCharacter(&header, &scPacket, refSectorVector[i]->dwSessionID);
			bool bRet = Send_UniCast(player->dwSessionID, &header, scPacket.GetBufferPtr());
			if (!bRet)
				continue;

			scPacket.Clear();

			_LOG(0, L"Delete Character # ID : %d\n", refSectorVector[i]->dwSessionID);
		}
	}

	// 2. AddSector에게 생성 메세지
	for (int i = 0; i < addSector.iCount; i++)
	{
		// 그 섹터의 캐릭터들 생성 - player에게 전송
		vector<st_CHARACTER*>& refSectorVector = m_Sector[addSector.Around[i].iY][addSector.Around[i].iX];
		for (int i = 0; i < refSectorVector.size(); i++)
		{
			if (refSectorVector[i]->bDeleted)
				continue;

			scPacket.Clear();
			mpCreateOtherCharacter(&header, &scPacket, refSectorVector[i]->dwSessionID, refSectorVector[i]->byDirection, 
				refSectorVector[i]->shX, refSectorVector[i]->shY, refSectorVector[i]->chHP);
			bool bRet = Send_UniCast(player->dwSessionID, &header, scPacket.GetBufferPtr());
			if (!bRet)
			{
				continue;
			}

			scPacket.Clear();
			mpCreateOtherCharacter(&header, &scPacket, player->dwSessionID, player->byDirection,
				player->shX, player->shY, player->chHP);
			Send_UniCast(refSectorVector[i]->dwSessionID, &header, scPacket.GetBufferPtr());
		}
	}

	// 3. map, player 세팅
	vector<st_CHARACTER*>& refSectorVector = m_Sector[player->OldSector.iY][player->OldSector.iX];
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		if (refSectorVector[i] == player)
		{
			refSectorVector.erase(refSectorVector.begin() + i);
			break;
		}
	}
	m_Sector[player->CurSector.iY][player->CurSector.iX].push_back(player);

	player->OldSector = player->CurSector;
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
		// 우측상단
		startX = 0;
		endX = 1;

		startY = 0;
		endY = 1;
	}
	else if (pivotX < centerX && pivotY >= centerY)
	{
		// 좌측상단
		startX = -1;
		endX = 0;

		startY = 0;
		endY = 1;
	}
	else if (pivotX >= centerX && pivotY < centerY)
	{
		// 우측하단
		startX = 0;
		endX = 1;

		startY = -1;
		endY = 0;
	}
	else if (pivotX < centerX && pivotY < centerY)
	{
		// 좌측하단
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
	// 1. OldSector, CurSector의 Around 구하기
	st_SECTOR_AROUND oldAround;
	st_SECTOR_AROUND curAround;

	GetSectorAround(player->OldSector.iX, player->OldSector.iY, &oldAround);
	GetSectorAround(player->CurSector.iX, player->CurSector.iY, &curAround);

	// 2. OldSector엔 있지만 CurSector엔 없는 섹터는 RemoveSector에 추가
	for (int oldIdx = 0; oldIdx < oldAround.iCount; oldIdx++)
	{
		bool bFlag = false;
		int iOldSectorX = oldAround.Around[oldIdx].iX;
		int iOldSectorY = oldAround.Around[oldIdx].iY;

		for (int curIdx = 0; curIdx < curAround.iCount; curIdx++)
		{
			int iCurSectorX = curAround.Around[curIdx].iX;
			int iCurSectorY = curAround.Around[curIdx].iY;

			// CurSector에 있다
			if (iCurSectorX == iOldSectorX && iCurSectorY == iOldSectorY)
			{
				bFlag = true;
				break;
			}
		}

		// OldSector엔 있지만 CurSector에 없으니까 RemoveSector다.
		if (!bFlag)
		{
			pRemoveSector->Around[pRemoveSector->iCount].iX = iOldSectorX;
			pRemoveSector->Around[pRemoveSector->iCount].iY = iOldSectorY;
			pRemoveSector->iCount++;
		}
	}

	// 3. OldSector엔 없지만 CurSector엔 있는 섹터는 AddSector에 추가
	for (int curIdx = 0; curIdx < curAround.iCount; curIdx++)
	{
		bool bFlag = false;
		int iCurSectorX = curAround.Around[curIdx].iX;
		int iCurSectorY = curAround.Around[curIdx].iY;

		for (int oldIdx = 0; oldIdx < oldAround.iCount; oldIdx++)
		{
			int iOldSectorX = oldAround.Around[oldIdx].iX;
			int iOldSectorY = oldAround.Around[oldIdx].iY;

			// OldSector에 있다
			if (iCurSectorX == iOldSectorX && iCurSectorY == iOldSectorY)
			{
				bFlag = true;
				break;
			}
		}

		// CurSector엔 있지만 OldSector에 없으니까 AddSector다.
		if (!bFlag)
		{
			pAddSector->Around[pAddSector->iCount].iX = iCurSectorX;
			pAddSector->Around[pAddSector->iCount].iY = iCurSectorY;
			pAddSector->iCount++;
		}
	}
}

void SendPacket_SectorOne(int iSectorX, int iSectorY, st_PACKET_HEADER* header, CPacket* cPacket, DWORD dwExceptSessionID)
{
	Profiler("SendPacket_SectorOne");
	vector<st_CHARACTER*>& refSectorVector = m_Sector[iSectorY][iSectorX];
	for (int i = 0; i < refSectorVector.size(); i++)
	{
		if (refSectorVector[i]->dwSessionID == dwExceptSessionID)
			continue;

		Send_UniCast(refSectorVector[i]->dwSessionID, header, cPacket->GetBufferPtr());
	}
}

void SendPacket_Around(st_CHARACTER* pCharacter, st_PACKET_HEADER* header, CPacket* cPacket, bool bSendMe)
{
	Profiler("SendPacket_Around");
	int iSectorX = pCharacter->shX / dfSECTOR_SIZE_X;
	int iSectorY = pCharacter->shY / dfSECTOR_SIZE_Y;

	st_SECTOR_AROUND stAround;
	GetSectorAround(iSectorX, iSectorY, &stAround);

	for (int i = 0; i < stAround.iCount; i++)
	{
		bool playerSectorFlag = false;
		bool bSendFlag = false;
		if (stAround.Around[i].iY == iSectorY && stAround.Around[i].iX == iSectorX)
			playerSectorFlag = true;

		vector<st_CHARACTER*>& refSectorVector = m_Sector[stAround.Around[i].iY][stAround.Around[i].iX];

		// if문 체크를 적게 하기 위해 나눠봄
		if (playerSectorFlag)
		{
			for (int i = 0; i < refSectorVector.size(); i++)
			{
				if (!bSendMe && refSectorVector[i]->dwSessionID == pCharacter->dwSessionID)
					continue;

				Send_UniCast(refSectorVector[i]->dwSessionID, header, cPacket->GetBufferPtr());
				/*bSendFlag = Send_UniCast(refSectorVector[i]->dwSessionID, header, cPacket->GetBufferPtr());
				if (!bSendFlag)
				{
					refSectorVector[i]->bDeleted = true;
				}*/
			}
		}
		else
		{
			for (int i = 0; i < refSectorVector.size(); i++)
			{
				Send_UniCast(refSectorVector[i]->dwSessionID, header, cPacket->GetBufferPtr());
				/*bSendFlag = Send_UniCast(refSectorVector[i]->dwSessionID, header, cPacket->GetBufferPtr());
				if (!bSendFlag)
				{
					refSectorVector[i]->bDeleted = true;
				}*/
			}
		}
	}
}