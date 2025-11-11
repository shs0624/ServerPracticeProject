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