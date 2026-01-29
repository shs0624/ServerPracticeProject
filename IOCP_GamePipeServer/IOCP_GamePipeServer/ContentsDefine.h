#pragma once
#include "Includes.h"

enum RoomMessageType
{
	ENTER,
	MESSAGE,
	LEAVE
};

struct stRoomMessage
{
	RoomMessageType type;
	ULONGLONG sessionID;
	RefCountPointer cPacket;
};