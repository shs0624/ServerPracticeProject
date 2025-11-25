#include "Includes.h"		
#include "NetServer.h"
#include "ChatServer.h"
#include "LogManager.h"

// time 측정을 위한 함수
void ChatServer::TimeCheck(DWORD& sleepTime)
{
	DWORD nowTime = timeGetTime();
	for (auto it = _SessionMap.begin(); it != _SessionMap.end(); it++)
	{
		st_SESSION* pSession = (*it).second;
		DWORD timeDiff = nowTime - pSession->dwLastRecvTime;
		if (timeDiff >= dfTIMEOUT_SESSION)
		{
			Disconnect(pSession->ulSessionID);
			LogController::_LogController._dwTimeoutSessionTotal++;
			continue;
		}

		if (timeDiff < sleepTime)
			sleepTime = timeDiff;
	}

	nowTime = timeGetTime();
	for (auto it = _UserMap.begin(); it != _UserMap.end(); it++)
	{
		st_USER* pUser = (*it).second;
		DWORD timeDiff = nowTime - pUser->dwLastRecvTime;
		if (timeDiff >= dfTIMEOUT_USER)
		{
			Disconnect(pUser->ulSessionID);
			LogController::_LogController._dwTimeoutUserTotal++;
			continue;
		}

		if (timeDiff < sleepTime)
			sleepTime = timeDiff;
	}
}

// 프레임 스킵 함수
bool ChatServer::Skip()
{

}
