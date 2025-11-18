#pragma once
#include "Includes.h"
#include "PacketDefine.h"
#include "CommonProtocol.h"
#include "DummyHandler.h"
#include "TCPNetwork.h"
#include "LogController.h"
#include "ChatDummy.h"
#include "ChatDummyController.h"

void ChatDummyController::InitController(int sessionCount, int startIdx, bool bTestTimeout)
{
	_iSessionCount = sessionCount;
	INT64 idx;
	for (idx = 0; idx < _iSessionCount; idx++)
	{
		//@@TODO : 파일에서 ID, 닉네임, AccountNo 읽어오는 방향으로 수정하기
		INT64 id = startIdx + idx;
		INT64 nick = id + 100000;

		_DummyArr[idx].Init(en_Normal, id, nick);
	}

	if (bTestTimeout)
	{
		for (int i = 0; i < dfTIMEOUTTEST_COUNT; i++)
		{
			INT64 id = startIdx + idx;
			INT64 nick = id + 100000;

			_DummyArr[idx++].Init(en_TimeOut_Session, id, nick);
		}

		for (int i = 0; i < dfTIMEOUTTEST_COUNT; i++)
		{
			INT64 id = startIdx + idx;
			INT64 nick = id + 100000;

			_DummyArr[idx++].Init(en_TimeOut_User, id, nick);
		}

		_iSessionCount += dfTIMEOUTTEST_COUNT * 2;
	}
}

void ChatDummyController::Update()
{
	DWORD nowTime = timeGetTime();
	for (int i = 0; i < _iSessionCount; i++)
	{
		if (!TimerErrorCheck(i))
			continue;

		HeartBeatProc(i, nowTime);

		if (_DummyArr[i].IsWait())
			continue;

		RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
		(*cPacket)->Initialize(MAX_PROTOCOLSIZE, sizeof(st_NetHeader));

		DummyAction act = _DummyArr[i].GetNextAction();
		_DummyArr[i].Update(cPacket);
		
		switch (act)
		{
		case en_ActionConnect:
			_dummyHandler->RequestConnect(i);
			break;
		case en_ActionDisconnect:
			_dummyHandler->RequestDisconnect(i);
			break;
		case en_ActionLogin:
			_DummyArr[i].OnSend();
			_dummyHandler->RequestSendPacket(i, cPacket);
			LogController::_LogController.LOG_SEND(_DummyArr[i]._AccountNo, en_PACKET_CS_CHAT_REQ_LOGIN);
			InterlockedIncrement(&LogController::_LogController._dwLoginSendCount);
			break;
		case en_ActionMove:
			_DummyArr[i].OnSend();
			_dummyHandler->RequestSendPacket(i, cPacket);
			LogController::_LogController.LOG_SEND(_DummyArr[i]._AccountNo, en_PACKET_CS_CHAT_REQ_SECTOR_MOVE);
			InterlockedIncrement(&LogController::_LogController._dwMoveSendCount);
			break;
		case en_ActionChat:
			_DummyArr[i].OnSend();
			_dummyHandler->RequestSendPacket(i, cPacket);
			LogController::_LogController.LOG_SEND(_DummyArr[i]._AccountNo, en_PACKET_CS_CHAT_REQ_MESSAGE);
			InterlockedIncrement(&LogController::_LogController._dwChatSendCount);
			break;
		}
	}
}

// 타임아웃 조건 체크 - 에러라면 FALSE
bool ChatDummyController::TimerErrorCheck(DWORD sessionID)
{
	DWORD nowTime = timeGetTime();
	ChatDummy* dummy = &_DummyArr[sessionID];
	ERROR_TYPE errorType = dummy->TimerCheck(nowTime);
	if (errorType == SUCCESS)
		return TRUE;
	
	switch (errorType)
	{
		case TIMEOUT_NOTRECV:
		{
			_InterlockedIncrement(&LogController::_LogController._dwMessageNotRecvCount);
			break;
		}
		case TIMEOUT_NOTRECV_LOGIN:
		{
			_InterlockedIncrement(&LogController::_LogController._dwLoginResNotRecvCount);
			break;
		}
		case NEED_TIMEOUT_USER:
		{
			_InterlockedIncrement(&LogController::_LogController._dwNeedTimeoutUserCount);
			break;
		}
		case NEED_TIMEOUT_SESSION:
		{
			_InterlockedIncrement(&LogController::_LogController._dwNeedTimeoutSessionCount);
			break;
		}
	}

	_dummyHandler->RequestDisconnect(sessionID);
	return FALSE;
}

void ChatDummyController::HeartBeatProc(DWORD sessionID, DWORD nowTime)
{
	DWORD lastHeartBeat = _DummyArr[sessionID]._dwLastHeartBeat;

	if (_DummyArr[sessionID].CheckHeartBeat(nowTime))
	{
		RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
		(*cPacket)->Initialize(MAX_PROTOCOLSIZE, sizeof(st_NetHeader));

		mpREQHeartBeat(cPacket);

		_DummyArr[sessionID]._dwLastHeartBeat = timeGetTime();

		_dummyHandler->RequestSendPacket(sessionID, cPacket);
	}
}

void ChatDummyController::OnConnect(DWORD sessionID)
{
	_DummyArr[sessionID].OnConnect();
}

void ChatDummyController::OnDisconnect(DWORD sessionID)
{
	_DummyArr[sessionID].OnDisconnect();
}

void ChatDummyController::OnRecv(DWORD sessionID, RefCountPointer& cPacket)
{
	// 더미 컨트롤러에게 메시지 넘기기
	if (!PacketProc(_DummyArr[sessionID], cPacket))
	{
		InterlockedIncrement(&LogController::_LogController._dwResponseFailCount);
	}
}

// 패킷을 해체 분석, 틀린 데이터라면 false 반환
bool ChatDummyController::PacketProc(ChatDummy& dummy, RefCountPointer& cPacket)
{
	WORD type;
	(*cPacket)->GetData((char*)&type, sizeof(WORD));

	BOOL flag = false;
	switch (type)
	{
		case en_PACKET_CS_CHAT_RES_LOGIN:
		{
			INT64 buffer;
			(*cPacket)->GetData((char*)&buffer, sizeof(BYTE));
			if ((BYTE)buffer != 1)
				return false;
			(*cPacket)->GetData((char*)&buffer, sizeof(INT64));
			if ((INT64)buffer != dummy._AccountNo)
				return false;

			//LogController::_LogController.LOG_RECV(dummy._AccountNo, buffer, en_PACKET_CS_CHAT_RES_LOGIN);
			dummy._bUser = TRUE;
			dummy._bWait = FALSE;
			return true;
		}
		case en_PACKET_CS_CHAT_RES_MESSAGE:
		{
			// 내가 보낸 채팅에 대한 응답이 오면 Wait을 해제. 나머진 패스
			INT64 buffer;
			(*cPacket)->GetData((char*)&buffer, sizeof(INT64));
			if ((INT64)buffer == dummy._AccountNo)
			{
				dummy._bWait = FALSE;
			}

			LogController::_LogController.LOG_RECV(dummy._AccountNo, buffer, en_PACKET_CS_CHAT_RES_MESSAGE);
			return true;
		}
		case en_PACKET_CS_CHAT_RES_SECTOR_MOVE:
		{
			INT64 buffer;
			(*cPacket)->GetData((char*)&buffer, sizeof(INT64));
			if ((INT64)buffer != dummy._AccountNo)
				return false;
			(*cPacket)->GetData((char*)&buffer, sizeof(WORD));
			if ((WORD)buffer != dummy._shSectorX)
				return false;
			(*cPacket)->GetData((char*)&buffer, sizeof(WORD));
			if ((WORD)buffer != dummy._shSectorY)
				return false;

		
			//LogController::_LogController.LOG_RECV(dummy._AccountNo, en_PACKET_CS_CHAT_RES_SECTOR_MOVE);
			dummy._bWait = FALSE;
			return true;
		}
	}

	return false;
}