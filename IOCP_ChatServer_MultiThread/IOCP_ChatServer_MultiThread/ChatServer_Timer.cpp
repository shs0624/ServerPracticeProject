#include "Includes.h"		
#include "NetServer.h"
#include "ChatServer.h"
#include "LogManager.h"

unsigned int WINAPI ChatServer::TimerThread(LPVOID arg)
{
	ChatServer* thisPtr = (ChatServer*)arg;

	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	HANDLE hHandleArr[2] = { thisPtr->_hQuitEvent, thisPtr->_hTimeoutEvent };

	DWORD ret = 0;
	while (1)
	{
		thisPtr->TimeCheck(dfSLEEPTIME);

		ret = WaitForMultipleObjects(2, hHandleArr, FALSE, dfSLEEPTIME);
		if (ret == WAIT_OBJECT_0)
		{
			// 서버 종료
			return 0;
		}
	}
}

// time 측정을 위한 함수
void ChatServer::TimeCheck(DWORD sleepTime)
{
	AcquireSRWLockShared(&_SessionMapLock);
	for (auto it = _SessionMap.begin(); it != _SessionMap.end(); it++)
	{
		st_SESSION* pSession = (*it).second;
		DWORD timeDiff = timeGetTime() - pSession->dwLastRecvTime;
		if (timeDiff >= dfTIMEOUT_SESSION)
		{
			Disconnect(pSession->ulSessionID);
			_pLog._dwTimeoutSessionTotal++;
			continue;
		}
	}
	ReleaseSRWLockShared(&_SessionMapLock);

	AcquireSRWLockShared(&_UserMapLock);
	for (auto it = _UserMap.begin(); it != _UserMap.end(); it++)
	{
		st_USER* pUser = (*it).second;
		DWORD timeDiff = timeGetTime() - pUser->dwLastRecvTime;
		if (timeDiff >= dfTIMEOUT_USER)
		{
			Disconnect(pUser->ulSessionID);
			_pLog._dwTimeoutUserTotal++;
			continue;
		}
	}
	ReleaseSRWLockShared(&_UserMapLock);
}