#pragma once

// 플레이어 삭제
void SetDeleteCharacter(DWORD dwSessionID);

// 성능을 위해 캐릭터 바로 넘기는 세션 삭제
void SetDeleteCharacter_Direct(st_CHARACTER* ptr);

// 컨텐츠 관련 구현
void CollisionCheck(st_CHARACTER* pCenterPlayer, char chDir, BYTE xRange, BYTE yRange, char chDamage);

// 범위 체크
bool CharacterMoveCheck(short shX, short shY);