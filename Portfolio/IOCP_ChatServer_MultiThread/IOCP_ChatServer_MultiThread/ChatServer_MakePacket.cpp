#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"

void ChatServer::mpRESLogin(RefCountPointer& cPacket, BYTE status, INT64 accountNum)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_CHAT_RES_LOGIN;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << status;
	(**cPacket) << accountNum;
}

void ChatServer::mpRESSectorMove(RefCountPointer& cPacket, INT64 accountNum, WORD sectorX, WORD sectorY)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << accountNum;
	(**cPacket) << sectorX;
	(**cPacket) << sectorY;
}

void ChatServer::mpRESMessage(RefCountPointer& cPacket, INT64 accountNum, WCHAR* id, WCHAR* nick, WORD len, WCHAR* message)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_CHAT_RES_MESSAGE;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << accountNum;

	(*cPacket)->PutData((char*)id, sizeof(WCHAR) * 20);
	(*cPacket)->PutData((char*)nick, sizeof(WCHAR) * 20);

	(**cPacket) << len;
	(*cPacket)->PutData((char*)message, len);
}
