#include "TCPDefine.h"
#include "PacketDefine.h"
#include "TCPNetwork.h"
#include "FrameProc.h"

bool m_bShutdown = false;

int wmain(int argc, WCHAR* argv[])
{
	timeBeginPeriod(1);

	//네트워크 세팅
	netStartup();

	while (!m_bShutdown)
	{
		// Select IO작업
		netSelectIO();

		// 프레임 업데이트
		Update();
	}

	timeEndPeriod(1);
}