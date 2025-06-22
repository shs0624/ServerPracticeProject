#include <iostream>
#include <Windows.h>
#include "LogProc.h"

int g_iLogLevel = 1;
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

void FPS()
{
	static DWORD Tick = timeGetTime();

	if (timeGetTime() - Tick >= 1000)
	{
		//cs_MoveCursor(0, HEIGHT + 3);
		printf("logic Frame : %d # selectIO Frame : %d\n", _logicFrame, _selectIOFrame);
		_logicFrame = 0;
		_selectIOFrame = 0;

		Tick += 1000;
	}
}