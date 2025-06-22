#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#define SERVERIP "127.0.0.1"
#define dfMAX_CONNECT 20000

void netStartup();
void netSelectIO();

bool bSessionAlive(DWORD dwsessionID);

// 외부 선언 - MessageProc
void ProcessMessage(DWORD dwsessionID, BYTE type, CPacket* cPacket);

//void GetSession()

void DisconnectSession(DWORD dwsessionID);
void DisconnectDeletedSession();

void Send_BroadCast(DWORD dwsessionID, st_PACKET_HEADER* header, char* packet);
bool Send_UniCast(DWORD dwsessionID, st_PACKET_HEADER* header, char* packet);
//void SendPacket_SectorOne(int iSectorX, int iSectorY, CPacket* cPacket, st_SESSION* pExceptSession);
//void SendPacket_Around(st_SESSION* pSession, CPacket* cPacket, bool bSendMe = false);