#include <WinSock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iostream>
#include "LogProc.h"
#include "MonitorManager.h"
#include "TCPDefine.h"
#include "PacketDefine.h"
#include "FrameProc.h"
#include "TCPNetwork.h"


extern int g_iLogLevel;
extern WCHAR g_szLogBuff[1024];

extern int _logicFrame;
extern int _selectIOFrame;


void Monitor()
{
	static DWORD Tick = timeGetTime();

	if (timeGetTime() - Tick >= 1000)
	{
		//cs_MoveCursor(0, HEIGHT + 3);
		printf("logic Frame : %d # selectIO Frame : %d\n", _logicFrame, _selectIOFrame);
		_logicFrame = 0;
		_selectIOFrame = 0;

		Tick += 1000;

		printf("sessionCount : %d # CharacterCount : %d\n\n", GetSessionCount(), GetCharacterCount());

		printf("----------------------------------------------------------------------------\n\n");
	}
}