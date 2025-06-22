#pragma once

void SetUserToSector(st_CHARACTER* player);

void SendUserInfoToNewPlayer(DWORD dwNewSessionID, short shX, short shY);

void DeletePlayerFromSector(DWORD dwSessionID, short shX, short shY);

bool UpdateSector(st_CHARACTER* player);

void ChangeSector(st_CHARACTER* player);

void GetSectorSessions(short shX, short shY, list<st_SESSION*> pPlayerList);

void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);

void GetUpdateSectorAround(st_CHARACTER* player, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector);

void SendPacket_SectorOne(int iSectorX, int iSectorY, st_PACKET_HEADER* header, CPacket* cPacket, DWORD dwExceptSessionID);

void SendPacket_Around(st_CHARACTER* pCharacter, st_PACKET_HEADER* header, CPacket* cPacket, bool bSendMe = false);