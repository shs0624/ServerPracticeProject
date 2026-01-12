#include "Includes.h"
#include "LanServer.h"
#include "MonitorProtocol.h"
#include "MonitorClientServer.h"
#include "MonitorDataManager.h"


void MonitorClientServer::MessageProc_MonitorClientLogin(RefCountPointer& cPacket, ULONGLONG sessionID)
{
	char loginSessionKey[32];
	(*cPacket)->GetData((char*)loginSessionKey, sizeof(loginSessionKey));

	// 로그인..
	AcquireSRWLockExclusive(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	if (it != _UserMap.end())
	{
		// 중복 로그인 처리 기존 것 삭제
		Disconnect((*it).second->ulSessionID);
	}
	ReleaseSRWLockExclusive(&_UserMapLock);

	st_ClientUSER* userPtr = _UserPool->Alloc();

	userPtr->ulSessionID = sessionID;
	userPtr->dwLastRecvTime = timeGetTime();
	memcpy(userPtr->SessionKey, loginSessionKey, sizeof(userPtr->SessionKey));

	AcquireSRWLockExclusive(&_UserMapLock);
	_UserMap.insert({ userPtr->ulSessionID, userPtr });
	ReleaseSRWLockExclusive(&_UserMapLock);

	AcquireSRWLockExclusive(&_SessionMapLock);
	auto itSession = _SessionMap.find(sessionID);
	if (itSession != _SessionMap.end())
	{
		st_ClientSESSION* ptr = (*itSession).second;

		userPtr->ClientAddr = ptr->ClientAddr;
		_SessionMap.erase(sessionID);
		_SessionPool->Free(ptr);
	}
	ReleaseSRWLockExclusive(&_SessionMapLock);

	// 기존 데이터를 전부 보내야하지 않을까~
	UpdateAll();

	_pLog._dwClientCount++;
}

void MonitorClientServer::mpDataUpdate(RefCountPointer& cPacket, BYTE serverNum, BYTE dataType, int dataValue, int timeStamp)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << serverNum;
	(**cPacket) << dataType;
	(**cPacket) << dataValue;
	(**cPacket) << timeStamp;
}