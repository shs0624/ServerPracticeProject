#pragma once
#define dfLOG_LEVEL_DEBUG 0
#define dfLOG_LEVEL_ERROR 1
#define dfLOG_LEVEL_SYSTEM 2

#define _LOG(LogLevel, fmt, ...)					\
do {												\
	if(g_iLogLevel <= LogLevel)						\
	{												\
		wsprintf(g_szLogBuff, fmt, ##__VA_ARGS__);	\
		Log(g_szLogBuff);							\
	}												\
}while(0)											\

void Log(WCHAR * szString);
void FPS();