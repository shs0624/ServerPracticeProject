#pragma once
#include "CommonProtocol.h"
#include "PacketDefine.h"
#include "Includes.h"
#include "ChatDummyManager_MakePacket.h"

void mpREQLogin(RefCountPointer cPacket, INT64 accountNo, WCHAR* id, WCHAR* nickname, char* sessionKey)
{
	// 40 + 40 + 64 - id, nickname, sessionKey
	st_NetHeader header;
	header.FixedKey = FIXED_KEY;
	header.RandKey = rand() % 256;
	header.shLen = sizeof(WORD) + sizeof(INT64) + 144;
	en_PACKET_TYPE type = en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_LOGIN;

	(*cPacket)->PutData((char*)&type, sizeof(type));
	(*cPacket)->PutData((char*)&accountNo, sizeof(INT64));
	(*cPacket)->PutData((char*)&id, sizeof(WCHAR) * 20);
	(*cPacket)->PutData((char*)nickname, sizeof(WCHAR) * 20);
	(*cPacket)->PutData((char*)sessionKey, sizeof(char) * 64);

	// 여기서 체크섬까지 다 넣고 인코딩해줌
	(*cPacket)->Encode(FIXED_KEY);
	(*cPacket)->PushHeader((char*)&header, sizeof(st_NetHeader));
}

void mpREQSectorMove(RefCountPointer cPacket, INT64 accountNo, WORD sectorX, WORD sectorY)
{
	st_NetHeader header;
	header.FixedKey = FIXED_KEY;
	header.RandKey = rand() % 256;
	header.shLen = sizeof(WORD) + sizeof(INT64) + sizeof(WORD) + sizeof(WORD);

	(*cPacket)->PutData((char*)en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_SECTOR_MOVE, sizeof(en_PACKET_TYPE));
	(*cPacket)->PutData((char*)&accountNo, sizeof(INT64));
	(*cPacket)->PutData((char*)&sectorX, sizeof(WORD));
	(*cPacket)->PutData((char*)&sectorY, sizeof(WORD));

	(*cPacket)->Encode(FIXED_KEY);
	(*cPacket)->PushHeader((char*)&header, sizeof(st_NetHeader));
}

void mpREQMessage(RefCountPointer cPacket, INT64 accountNo, WORD messageLen, WCHAR* message)
{
	st_NetHeader header;
	header.FixedKey = FIXED_KEY;
	header.RandKey = rand() % 256;
	header.shLen = sizeof(WORD) + sizeof(INT64) + sizeof(WORD) + messageLen;

	(*cPacket)->PutData((char*)en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_MESSAGE, sizeof(en_PACKET_TYPE));
	(*cPacket)->PutData((char*)&accountNo, sizeof(INT64));
	(*cPacket)->PutData((char*)&messageLen, sizeof(WORD));
	(*cPacket)->PutData((char*)message, messageLen);

	(*cPacket)->Encode(FIXED_KEY);
	(*cPacket)->PushHeader((char*)&header, sizeof(st_NetHeader));
}

void mpREQHeartBeat(RefCountPointer cPacket)
{
	st_NetHeader header;
	header.FixedKey = FIXED_KEY;
	header.RandKey = rand() % 256;
	header.shLen = sizeof(WORD);

	en_PACKET_TYPE type = en_PACKET_TYPE::en_PACKET_CS_CHAT_REQ_HEARTBEAT;

	(*cPacket)->PutData((char*)&type, sizeof(en_PACKET_TYPE));

	(*cPacket)->Encode(FIXED_KEY);
	(*cPacket)->PushHeader((char*)&header, sizeof(st_NetHeader));
}