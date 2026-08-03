#include "Includes.h"
#include "NetServer.h"
#include "LanServer.h"
#include "LogManager.h"
#include "MonitorProtocol.h"
#include "MonitorClientServer.h"
#include "MonitorDataManager.h"


void MonitorClientServer::MessageProc_MonitorClientLogin(RefCountPointer& cPacket, ULONGLONG sessionID)
{
	BYTE status;
	char loginSessionKey[32];
	(*cPacket)->GetData((char*)loginSessionKey, sizeof(loginSessionKey));

	if (strncmp(loginSessionKey, dfCLIENT_SESSIONKEY, sizeof(loginSessionKey)) != 0)
	{
		// 세션 키가 달라 로그인 거부, 연결 끊기
		AcquireSRWLockExclusive(&_SessionMapLock);
		auto itSession = _SessionMap.find(sessionID);
		if (itSession != _SessionMap.end())
		{
			st_ClientSESSION* ptr = (*itSession).second;
			_SessionMap.erase(sessionID);
			_SessionPool->Free(ptr);
		}
		ReleaseSRWLockExclusive(&_SessionMapLock);
			
		// 실패 패킷 전송 준비
		(*cPacket)->Clear(sizeof(st_NetHeader));

		status = dfMONITOR_TOOL_LOGIN_ERR_SESSIONKEY;
		mpLoginRES(cPacket, status);
		SendPacket_UniCast(sessionID, cPacket);

		Disconnect(sessionID);
		return;
	}

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

	// 성공 패킷을 보내니 클라가 뻗어서 일단 보류
	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;

	_pLog._dwClientCount++;
}

void MonitorClientServer::mpLoginRES(RefCountPointer& cPacket, BYTE status)
{
	en_PACKET_TYPE packetType = en_PACKET_CS_MONITOR_TOOL_RES_LOGIN;

	(**cPacket) << status;
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