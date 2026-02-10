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

// 로그인 하지 않은 세션
struct st_SESSION
{
	ULONGLONG ulSessionID;
	SOCKADDR_IN ClientAddr;

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
};

// 로그인 한 유저
struct st_USER
{
	ULONGLONG ulSessionID;
	SOCKADDR_IN ClientAddr;

	INT64 AccountNum;
	char SessionKey[64];

	// 타임아웃용 시간
	DWORD dwLastRecvTime;

	// 공격 메세지 체크용 카운터
	DWORD dwMessageAlertCount;
	DWORD dwDisconnectAlertCount;
};