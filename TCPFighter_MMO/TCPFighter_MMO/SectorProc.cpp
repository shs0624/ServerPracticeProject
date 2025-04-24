#include <list>
using namespace std;

#include "TCPDefine.h"
#include "SectorDefine.h"
#include "ContentsDefine.h"
#include "PacketDefine.h"
#include "SectorProc.h"
#include "MessageCreate.h"
#include "TCPNetwork.h"

// 한 섹터는 100 x 100 크기로,  64 x 64개의 섹터로 이루어짐
list<st_CHARACTER*> m_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

// 유저를 섹터에 세팅하고, 그 섹터의 타 유저 정보도 전송
void SetUserToSector(st_CHARACTER* player)
{
	short sectorX = (player->shX) / dfSECTOR_SIZE_X;
	short sectorY = (player->shY) / dfSECTOR_SIZE_Y;

	player->CurSector.iX = sectorX;
	player->CurSector.iY = sectorY;

	player->OldSector.iX = sectorX;
	player->OldSector.iY = sectorY;

	st_PACKET_HEADER header;
	CPacket* scPacket = new CPacket(PROTOCOL_MAXSIZE);

	list<st_CHARACTER*>::iterator it;
	list<st_CHARACTER*> liSectorPList;

	// 그 섹터의 유저들 정보를 새 유저에게 전송
	st_SECTOR_AROUND aroundSector;
	GetSectorAround(sectorX, sectorY, &aroundSector);

	for (int i = 0; i < aroundSector.iCount; i++)
	{
		int iX = aroundSector.Around[i].iX;
		int iY = aroundSector.Around[i].iY;

		liSectorPList = m_Sector[aroundSector.Around[i].iY][aroundSector.Around[i].iX];
		for (it = liSectorPList.begin(); it != liSectorPList.end(); it++)
		{
			mpCreateOtherCharacter(&header, scPacket, (*it)->dwSessionID, (*it)->byDirection, (*it)->shX, (*it)->shY, (*it)->chHP);
			Send_UniCast(player->pSession, &header, (char*)scPacket);
			scPacket->Clear();

			// 그 유저가 이동중이라면 MOVESTART도 전송
			if ((*it)->dwAction != dfPACKET_MOVE_DIR_NONE)
			{
				//scMoveStart
				mpMoveStart(&header, scPacket, (*it)->dwSessionID, (*it)->dwAction, (*it)->shX, (*it)->shY);
				Send_UniCast(player->pSession, &header, scPacket->GetBufferPtr());
				scPacket->Clear();
			}
		}
	}

	m_Sector[sectorY][sectorX].push_back(player);
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
	st_SECTOR_AROUND removeSector;
	st_SECTOR_AROUND addSector;

	removeSector.iCount = 0;
	addSector.iCount = 0;

	// player의 OldSector, Cursector가 다른채로 있어야 한다.
	GetUpdateSectorAround(player, &removeSector, &addSector);

	st_PACKET_HEADER header;
	CPacket* scPacket = new CPacket(PROTOCOL_MAXSIZE);

	list<st_CHARACTER*>::iterator it;
	list<st_CHARACTER*> liSectorPList;

	// 1. RemoveSector에게 Delete 메세지
	for (int i = 0; i < removeSector.iCount; i++)
	{
		liSectorPList = m_Sector[removeSector.Around[i].iY][removeSector.Around[i].iX];
		for (it = liSectorPList.begin(); it != liSectorPList.end(); it++)
		{
			mpDeleteCharacter(&header, scPacket, (*it)->dwSessionID);
			Send_UniCast(player->pSession, &header, (char*)scPacket);
			scPacket->Clear();
		}
	}

	// 2. AddSector에게 생성 메세지
	for (int i = 0; i < addSector.iCount; i++)
	{
		liSectorPList = m_Sector[addSector.Around[i].iY][addSector.Around[i].iX];
		for (it = liSectorPList.begin(); it != liSectorPList.end(); it++)
		{
			mpCreateOtherCharacter(&header, scPacket, (*it)->dwSessionID, (*it)->byDirection, (*it)->shX, (*it)->shY, (*it)->chHP);
			Send_UniCast(player->pSession, &header, (char*)scPacket);
			scPacket->Clear();
		}
	}

	// 3. map, player 세팅
	m_Sector[player->OldSector.iY][player->OldSector.iX].remove(player);
	m_Sector[player->CurSector.iY][player->CurSector.iX].push_back(player);

	player->OldSector = player->CurSector;
}

void GetAroundSessions(short shX, short shY, list<st_SESSION*> pPlayerList)
{
	
}

void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
{
	pSectorAround->iCount = 0;
	// 여기서 전부 밀어버려야되나?

	for (int iY = -1; iY <= 1; iY++)
	{
		if (iSectorY + iY < 0 || iSectorY >= dfSECTOR_MAX_Y)
			continue;

		for (int iX = -1; iX <= 1; iX++)
		{
			if (iSectorX + iX < 0 || iSectorX >= dfSECTOR_MAX_X)
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

void SendPacket_SectorOne(int iSectorX, int iSectorY, st_PACKET_HEADER* header, CPacket* cPacket, st_SESSION* pExceptSession)
{
	list<st_CHARACTER*>::iterator it;
	list<st_CHARACTER*> pSectorPlayerList;

	pSectorPlayerList = m_Sector[iSectorY][iSectorX];
	for (it = pSectorPlayerList.begin(); it != pSectorPlayerList.end(); it++)
	{
		if ((*it)->pSession == pExceptSession)
			continue;

		Send_UniCast((*it)->pSession, header, (char*)cPacket);
	}
}

void SendPacket_Around(st_CHARACTER* pCharacter, st_PACKET_HEADER* header, CPacket* cPacket, bool bSendMe)
{
	int iSectorX = pCharacter->shX / dfSECTOR_SIZE_X;
	int iSectorY = pCharacter->shY / dfSECTOR_SIZE_Y;

	st_SECTOR_AROUND stAround;

	GetSectorAround(iSectorX, iSectorY, &stAround);

	for (int i = 0; i < stAround.iCount; i++)
	{
		bool playerSectorFlag = false;
		list<st_CHARACTER*>::iterator it;
		list<st_CHARACTER*> pSectorPlayerList;

		if (stAround.Around[i].iY == iSectorY && stAround.Around[i].iX == iSectorX)
			playerSectorFlag = true;

		pSectorPlayerList = m_Sector[stAround.Around[i].iY][stAround.Around[i].iX];

		// if문 체크를 적게 하기 위해 나눠봄
		if (playerSectorFlag)
		{
			for (it = pSectorPlayerList.begin(); it != pSectorPlayerList.end(); it++)
			{
				if (!bSendMe && (*it)->dwSessionID == pCharacter->dwSessionID)
					continue;

				Send_UniCast((*it)->pSession, header, (char*)cPacket);
			}
		}
		else
		{
			for (it = pSectorPlayerList.begin(); it != pSectorPlayerList.end(); it++)
			{
				Send_UniCast((*it)->pSession, header, (char*)cPacket);
			}
		}
	}
}