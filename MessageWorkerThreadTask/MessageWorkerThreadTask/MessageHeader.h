#pragma once
#include <iostream>
#include <Windows.h>
#include <process.h>
#include <list>
#include <string>
#include <wchar.h>
#include "CRingBuffer.h"
using namespace std;

#define dfJOB_ADD	0
#define dfJOB_DEL	1
#define dfJOB_SORT	2
#define dfJOB_FIND	3
#define dfJOB_PRINT	4
#define dfJOB_QUIT	5	

struct st_MSG_HEAD
{
	short shType;
	short shPayloadLen;
};

