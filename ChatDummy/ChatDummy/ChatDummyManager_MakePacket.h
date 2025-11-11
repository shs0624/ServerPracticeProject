#pragma once

void mpREQLogin(RefCountPointer cPacket, INT64 accountNo, WCHAR* id, WCHAR* nickname, char* sessionKey);
void mpREQSectorMove(RefCountPointer cPacket, INT64 accountNo, WORD sectorX, WORD sectorY);
void mpREQMessage(RefCountPointer cPacket, INT64 accountNo, WORD messageLen, WCHAR* message);
void mpREQHeartBeat(RefCountPointer cPacket);