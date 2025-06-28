#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#define SERVERIP "127.0.0.1"
#define dfMAX_CONNECT 20000

void netStartup();
void netSelectIO();

bool bSessionAlive(DWORD dwsessionID);

// 외부 선언 - MessageProc
void ProcessMessage(st_SESSION* pSession, BYTE type, CPacket* cPacket);

int GetSessionCount();

void DisconnectSession(st_SESSION* pSession);
void DisconnectDeletedSession();

void Send_BroadCast(DWORD dwsessionID, st_PACKET_HEADER* header, char* packet);
bool Send_UniCast(st_SESSION* pSession, CPacket* cPacket);
//void SendPacket_SectorOne(int iSectorX, int iSectorY, CPacket* cPacket, st_SESSION* pExceptSession);
//void SendPacket_Around(st_SESSION* pSession, CPacket* cPacket, bool bSendMe = false);