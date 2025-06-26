#pragma once

void mpMoveStart(CPacket* msg, DWORD id, char dir, short X, short Y);
void mpMoveStop(CPacket* msg, DWORD id, char dir, short X, short Y);
void mpSync(CPacket* msg, DWORD id, short X, short Y);
void mpCreateMyCharacter(CPacket* msg, DWORD id, char dir, short X, short Y, char HP);
void mpCreateOtherCharacter(CPacket* msg, DWORD id, char dir, short X, short Y, char HP);
void mpDeleteCharacter(CPacket* msg, DWORD id);

void mpAttack1(CPacket* msg, DWORD id, char dir, short X, short Y);
void mpAttack2(CPacket* msg, DWORD id, char dir, short X, short Y);
void mpAttack3(CPacket* msg, DWORD id, char dir, short X, short Y);
void mpDamage(CPacket* msg, DWORD attackerID, DWORD damagedID, char damagedHP);
void mpEcho(CPacket* msg, DWORD time);