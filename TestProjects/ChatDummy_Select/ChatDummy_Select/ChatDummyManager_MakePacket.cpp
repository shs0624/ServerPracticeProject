#pragma once
#include "Includes.h"
#include "CommonProtocol.h"
#include "PacketDefine.h"
#include "ChatDummyManager_MakePacket.h"

void mpREQLogin(RefCountPointer cPacket, INT64 accountNo, WCHAR* id, WCHAR* nickname, char* sessionKey)
{
	en_PACKET_TYPE type = en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_LOGIN;

	(*cPacket)->PutData((char*)&type, sizeof(WORD));
	(*cPacket)->PutData((char*)&accountNo, sizeof(INT64));
	(*cPacket)->PutData((char*)id, sizeof(WCHAR) * 20);
	(*cPacket)->PutData((char*)nickname, sizeof(WCHAR) * 20);
	(*cPacket)->PutData((char*)sessionKey, sizeof(char) * 64);
}

void mpREQSectorMove(RefCountPointer cPacket, INT64 accountNo, WORD sectorX, WORD sectorY)
{
	en_PACKET_TYPE type = en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_SECTOR_MOVE;

	(*cPacket)->PutData((char*)&type, sizeof(WORD));
	(*cPacket)->PutData((char*)&accountNo, sizeof(INT64));
	(*cPacket)->PutData((char*)&sectorX, sizeof(WORD));
	(*cPacket)->PutData((char*)&sectorY, sizeof(WORD));
}

void mpREQMessage(RefCountPointer cPacket, INT64 accountNo, WORD messageLen, const WCHAR* message)
{
	en_PACKET_TYPE type = en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_MESSAGE;

	(*cPacket)->PutData((char*)&type, sizeof(WORD));
	(*cPacket)->PutData((char*)&accountNo, sizeof(INT64));
	(*cPacket)->PutData((char*)&messageLen, sizeof(WORD));
	(*cPacket)->PutData((char*)message, messageLen);
}

void mpREQHeartBeat(RefCountPointer cPacket)
{
	en_PACKET_TYPE type = en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_HEARTBEAT;

	(*cPacket)->PutData((char*)&type, sizeof(WORD));
}