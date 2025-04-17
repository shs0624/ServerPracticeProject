#pragma once

void InitializeSector(st_CHARACTER* player);

bool UpdateSector(st_CHARACTER* player);

void ChangeSector(st_CHARACTER* player);

void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);

void GetUpdateSectorAround(st_CHARACTER* player, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector);