#pragma once

void mpMoveStart(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y);
void mpMoveStop(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y);
void mpSync(st_PACKET_HEADER* header, CPacket* msg, DWORD id, short X, short Y);
void mpCreateMyCharacter(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y, char HP);
void mpCreateOtherCharacter(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y, char HP);
void mpDeleteCharacter(st_PACKET_HEADER* header, CPacket* msg, DWORD id);

void mpAttack1(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y);
void mpAttack2(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y);
void mpAttack3(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y);
void mpDamage(st_PACKET_HEADER* header, CPacket* msg, DWORD attackerID, DWORD damagedID, char damage);
void mpEcho(st_PACKET_HEADER* header, CPacket* msg, DWORD time);