#pragma once
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include "RefCountPointer.h"
#include "LockFreeQueue.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "ProcademyProfiler.h"
#include <conio.h>
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
	_MessageQueue->Enqueue(cPacket);
}

void ChatServer::OnError(int errorcode, WCHAR* message)
{

}

void ChatServer::MessageProc()
{
	int loopCnt = _MessageQueue->Size();
	for (int i = 0; i < loopCnt; i++)
	{
		RefCountPointer cPacket;
		if (!_MessageQueue->Dequeue(cPacket))
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

	while (1)
	{
		while (thisPtr->Skip())
		{
			thisPtr->Update();
		}

		// 메세지 큐에서 Dequeue후 작업
		thisPtr->MessageProc();

		thisPtr->DisconnectDeletedSession();
	}
}