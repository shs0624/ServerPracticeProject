#pragma once
// 메시지 프로시저 중간부에 해당한다.

// 외부 구현 - ContentsProc
bool netPacketProc_MoveStart(st_SESSION* session, CPacket* packet);
bool netPacketProc_MoveStop(st_SESSION* session, CPacket* packet);
bool netPacketProc_Attack1(st_SESSION* session);
bool netPacketProc_Attack2(st_SESSION* session);
bool netPacketProc_Attack3(st_SESSION* session);
bool netPacketProc_Echo(st_SESSION* session);
bool netPacketProc_Accept(st_SESSION* session);