#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"

int main()
{
	ChatServer* _chatServer = new ChatServer(INADDR_ANY, SERVERPORT, true, 500);

	char ch;
	while (1)
	{
		// 컨트롤?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			_chatServer->QuitServer();
			//break;
		}
		if (ch == 'P' || ch == 'p')
		{
			ProfileDataOutText("ProfileData.txt");
		}

	}
}

bool ChatServer::OnAccept(ULONGLONG SessionID)
{
	return true;
}

void ChatServer::OnRelease(ULONGLONG SessionID)
{

}

void ChatServer::OnRecv(ULONGLONG SessionID, RefCountPointer& cPacket)
{
	_MessageQ->Enqueue(cPacket);
}

void ChatServer::OnError(int errorcode, WCHAR* message)
{

}

void ChatServer::MessageProc()
{
	int loopCnt = _MessageQ->Size();
	for (int i = 0; i < loopCnt; i++)
	{
		RefCountPointer cPacket;
		if (!_MessageQ->Dequeue(cPacket))
		{
			DebugBreak();
		}

		// 메세지가 끊어진 유저의 것인지 체크 필요

		WORD type;
		(**cPacket) >> type;

		// enum에 따라 다른 메세지 처리 ... 추가 예정
		switch ((en_PACKET_TYPE)type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
			break;
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
			break;
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
			break;
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			break;
		}
	}
}

unsigned int WINAPI ChatServer::ContentsThread(LPVOID arg)
{
	ChatServer* thisPtr = (ChatServer*)arg;

	HANDLE hHandleArr[3] = { thisPtr->_hQuitEvent, thisPtr->_hMessageQueueEvent, thisPtr->_hTimeoutEvent};

	DWORD ret = 0;
	while (1)
	{
		// 프레임은 없어야 한다. 일이 있을때만 꺠어나서, 메세지를 처리해야 한다.
		ret = WaitForMultipleObjects(3, hHandleArr, FALSE, INFINITE);
		if (ret == WAIT_OBJECT_0)
		{
			// 서버 종료
		}
		else if (ret == _hMessageQueueEvent)
		{

		}
		// 그 외에는 메세지가 있어서 깨어난거임.

		// 메세지 큐에서 Dequeue후 작업
		thisPtr->MessageProc();

		thisPtr->DisconnectDeletedSession();
	}
}