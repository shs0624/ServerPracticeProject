#pragma once
#define MAX_PROTOCOLSIZE 155
#define MAX_MESSAGELEN 64
#define FIXED_KEY 0xa9

#define dfSECTOR_MAX_Y 32
#define dfSECTOR_MAX_X 32

struct st_NetHeader
{
	unsigned char FixedKey;
	short shLen;
	unsigned char RandKey;
	unsigned char CheckSum;
};

enum ERROR_TYPE
{
	SUCCESS = 0,
	TIMEOUT_NOTRECV,
	TIMEOUT_NOTRECV_LOGIN,
	NEED_TIMEOUT_USER,
	NEED_TIMEOUT_SESSION
};