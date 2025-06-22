#pragma once

// 플레이어 삭제
bool SetDeleteCharacter(DWORD dwSessionID);

// 컨텐츠 관련 구현
void CollisionCheck(st_CHARACTER* pCenterPlayer, char chDir, BYTE xRange, BYTE yRange, CStack<st_CHARACTER*>* pCheckedStack);

// 범위 체크
bool CharacterMoveCheck(short shX, short shY);