#pragma once
#include "CSerializationBuffer.h"
#include "CRingBuffer.h"
#include <Windows.h>

struct st_SESSION
{
	SOCKET Socket;			// 접속자의 TCP 소켓
	DWORD dwSessionID;		// 접속자의 고유 세션 ID
	CRingBuffer* RecvQ;		// 수신 큐
	CRingBuffer* SendQ;		// 송신 큐
	DWORD dwLastRecvTime;	// 타임아웃용 시간

	SOCKADDR_IN IPPtr;
	bool bDeleted;
};

//-----------------------------------------------------------------
// 30초 이상이 되도록 아무런 메시지 수신도 없는경우 접속 끊음.
//-----------------------------------------------------------------
#define dfNETWORK_PACKET_RECV_TIMEOUT	30000