#pragma once
#include <iostream>
#include <Windows.h>
#include <process.h>
#include <list>
#include <string>
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

//-----------------------------------------------
// 컨텐츠 부, 문자열 리스트
//-----------------------------------------------
list<wstring>		g_List;

//-----------------------------------------------
// 스레드 메시지 큐 (사이즈 넉넉하게 크게 4~5만 바이트)
//-----------------------------------------------
CRingBuffer* 		g_msgQ;

unsigned int WorkerThread(LPVOID lpParam);