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
		DWORD sleepTime = dfSLEEPTIME;

		thisPtr->TimeCheck(sleepTime);

		ret = WaitForMultipleObjects(2, hHandleArr, FALSE, sleepTime);
		if (ret == WAIT_OBJECT_0)
		{
			// 서버 종료
			return 0;
		}
	}
}

// time 측정을 위한 함수
void ChatServer::TimeCheck(DWORD& sleepTime)
{
	AcquireSRWLockShared(&_SessionMapLock);
	for (auto it = _SessionMap.begin(); it != _SessionMap.end(); it++)
	{
		st_SESSION* pSession = (*it).second;
		DWORD timeDiff = timeGetTime() - pSession->dwLastRecvTime;
		if (timeDiff >= dfTIMEOUT_SESSION)
		{
			ReleaseSRWLockShared(&_SessionMapLock);
			Disconnect(pSession->ulSessionID);
			AcquireSRWLockShared(&_SessionMapLock);
			_pLog._dwTimeoutSessionTotal++;
			continue;
		}

		if (timeDiff < sleepTime)
			sleepTime = timeDiff;
	}
	ReleaseSRWLockShared(&_SessionMapLock);

	AcquireSRWLockShared(&_UserMapLock);
	for (auto it = _UserMap.begin(); it != _UserMap.end(); it++)
	{
		st_USER* pUser = (*it).second;
		DWORD timeDiff = timeGetTime() - pUser->dwLastRecvTime;
		if (timeDiff >= dfTIMEOUT_USER)
		{
			ReleaseSRWLockShared(&_UserMapLock);
			Disconnect(pUser->ulSessionID);
			AcquireSRWLockShared(&_UserMapLock);
			_pLog._dwTimeoutUserTotal++;
			continue;
		}

		if (timeDiff < sleepTime)
			sleepTime = timeDiff;
	}
	ReleaseSRWLockShared(&_UserMapLock);
}

// 프레임 스킵 함수
bool ChatServer::Skip()
{

}
