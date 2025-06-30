#pragma once

void SetUserToSector(st_CHARACTER* player);

void SendUserInfoToNewPlayer(st_SESSION* pSession, short shX, short shY);

void DeletePlayerFromSector(DWORD dwSessionID, short shX, short shY);

bool UpdateSector(st_CHARACTER* player);

void ChangeSector(st_CHARACTER* player);

void GetSectorSessions(short sectorX, short sectorY, CStack<st_CHARACTER*>& pPlayerStack);

void GetDamageShowSector(int shX, int shY, st_SECTOR_AROUND* pSectorShowAttack);

void GetAttackTargetSector(int playerX, int playerY, BYTE dir, BYTE xRange, BYTE yRange, st_SECTOR_AROUND* pSectorAttack);

void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);

void GetUpdateSectorAround(st_CHARACTER* player, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector);

void SendPacket_SectorOne(int iSectorX, int iSectorY, CPacket* cPacket, DWORD dwExceptSessionID);

void SendPacket_Around(st_CHARACTER* pCharacter, CPacket* cPacket, bool bSendMe = false);