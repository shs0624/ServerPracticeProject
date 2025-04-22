#pragma once

// 컨텐츠 관련 구현
void CollisionCheck(st_CHARACTER* pExceptPlayer, char chDir, BYTE xRange, BYTE yRange, list<st_CHARACTER*> pCheckedList);

// 범위 체크
bool CharacterMoveCheck(short shX, short shY);