#pragma once
#include <iostream>
#include <Windows.h>
#include "TCPDefine.h"
#include "LogProc.h"

int g_iLogLevel = 2;
WCHAR g_szLogBuff[1024];

int _logicFrame = 0;
int _selectIOFrame = 0;

DWORD _iAcceptIdx = 0;
DWORD _iDisconnectIdx = 0;
st_SESSION _pAcceptLog[LOGMAX];
st_SESSION _pDisconnectLog[LOGMAX];

void Log(WCHAR* szString)
{
	time_t now = time(NULL);
	struct tm date;

	localtime_s(&date, &now);

	wprintf(L"[%02d/%02d/%02d %02d:%02d:%02d] ", date.tm_mon + 1, date.tm_mday, date.tm_year - 100, date.tm_hour, date.tm_min, date.tm_sec);
	wprintf(L"%s", szString);
}

void Log_Accept(st_SESSION* pSession)
{
	DWORD idx = (InterlockedIncrement(&_iAcceptIdx)) % LOGMAX;
	//_pAcceptLog[idx] = pSession;
	memcpy(&_pAcceptLog[idx], pSession, sizeof(st_SESSION));
}

void Log_Disconnect(st_SESSION* pSession)
{
	DWORD idx = (InterlockedIncrement(&_iDisconnectIdx)) % LOGMAX;
	//_pDisconnectLog[idx] = pSession;
	memcpy(&_pDisconnectLog[idx], pSession, sizeof(st_SESSION));
}