#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "winmm.lib")

#define SERVER_PORT 5000
#define SERVERIP "127.0.0.1"

void netStartup();
void netSelectIO();
void netProc_Accept();
void netProc_Recv(st_SESSION* session);
void netProc_Send(st_SESSION* session);

// 외부 선언 - MessageProc
void ProcessMessage(st_SESSION* session, BYTE type, CPacket* cPacket);

void DisconnectSession(SOCKET socket);

void Send_BroadCast(st_SESSION* exceptSession, st_PACKET_HEADER* header, char* packet);
void Send_UniCast(st_SESSION* pSession, st_PACKET_HEADER* header, char* packet);
//void SendPacket_SectorOne(int iSectorX, int iSectorY, CPacket* cPacket, st_SESSION* pExceptSession);
//void SendPacket_Around(st_SESSION* pSession, CPacket* cPacket, bool bSendMe = false);