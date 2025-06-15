#pragma once
// 메시지 프로시저 중간부에 해당한다.

// 외부 구현 - ContentsProc
bool netPacketProc_MoveStart(DWORD dwsessionID, CPacket* packet);
bool netPacketProc_MoveStop(DWORD dwsessionID, CPacket* packet);
bool netPacketProc_Attack(DWORD dwsessionID, BYTE type);
bool netPacketProc_Echo(DWORD dwsessionID, CPacket* packet);
bool netPacketProc_Accept(DWORD dwsessionID);