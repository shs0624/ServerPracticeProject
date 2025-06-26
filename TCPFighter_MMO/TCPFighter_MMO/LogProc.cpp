#include <iostream>
#include <Windows.h>
#include "LogProc.h"

int g_iLogLevel = 2;
WCHAR g_szLogBuff[1024];

int _logicFrame = 0;
int _selectIOFrame = 0;

void Log(WCHAR* szString)
{
	time_t now = time(NULL);
	struct tm date;

	localtime_s(&date, &now);

	wprintf(L"[%02d/%02d/%02d %02d:%02d:%02d] ", date.tm_mon + 1, date.tm_mday, date.tm_year - 100, date.tm_hour, date.tm_min, date.tm_sec);
	wprintf(L"%s", szString);
}