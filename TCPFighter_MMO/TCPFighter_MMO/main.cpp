#pragma comment(lib,"ws2_32")
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include "TCPDefine.h"
#include "PacketDefine.h"
#include "TCPNetwork.h"
#include "FrameProc.h"
#include "CCrashDump.h"
#include "ProcademyProfiler.h"
#include "LogProc.h"
#include "MonitorManager.h"
#include <conio.h>

HANDLE _controlThreadHandle;
unsigned int _controlThreadID;

procademy::CCrashDump cCrashDump;
bool m_bShutdown = false;

unsigned int WINAPI GetControl(LPVOID arg);
bool Skip();

int wmain(int argc, WCHAR* argv[])
{
	timeBeginPeriod(1);

	_controlThreadHandle = (HANDLE)_beginthreadex(NULL, 0, GetControl, NULL, 0, &_controlThreadID);
	if (_controlThreadHandle == NULL)
		return 1;

	//네트워크 세팅
	netStartup();

	while (!m_bShutdown)
	{
		// Select IO작업
		netSelectIO();

		// 프레임 업데이트
		while (Skip())
		{
			Update();
		}

		Monitor();
	}

	timeEndPeriod(1);
}

unsigned int WINAPI GetControl(LPVOID arg)
{
	// 컨트롤?
	char ch;
	while (1)
	{
		ch = _getch();
		if ((GetAsyncKeyState('P') & 0x8001) || (GetAsyncKeyState('p') & 0x8001))
		{
			ProfileDataOutText("ProfileData_TCPMMO_0626.txt");
		}
		if ((GetAsyncKeyState('R') & 0x8001) || (GetAsyncKeyState('r') & 0x8001))
		{
			ProfileReset();
		}
	}
}

bool Skip()
{
	static int _Tick = timeGetTime();

	int diff = timeGetTime() - _Tick;
	if (diff < 40)
	{
		return false;
	}
	else
	{
		_Tick += FRAME_TIME;
		return true;
	}
}