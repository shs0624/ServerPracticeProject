#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "CommonProtocol.h"
#include "CFreeList.h"
#include "LogManager.h"

int main()
{
	// 생성자 호출을 위한 GetInstance
	LogController::GetInstance();

	ChatServer* _chatServer = new ChatServer(INADDR_ANY, SERVERPORT, true, 5000);

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
	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	(*cPacket)->Initialize(PROTOCOL_MAX_SIZE, 0);
	_pLog._dwPacketPoolUse++;

	WORD workType = en_WORK_ACCEPT;
	(*cPacket)->PutData((char*)&workType, sizeof(workType));
	(*cPacket)->PutData((char*)&SessionID, sizeof(ULONGLONG));

	// 세션 Accept
	_MessageQ->Enqueue(cPacket);
	SetEvent(_hMessageQueueEvent);
	_pLog._dwUpdateQSize++;
	return true;
}

void ChatServer::OnRelease(ULONGLONG SessionID)
{
	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	(*cPacket)->Initialize(PROTOCOL_MAX_SIZE, 0);
	_pLog._dwPacketPoolUse++;

	WORD workType = en_WORK_RELEASE;
	(*cPacket)->PutData((char*)&workType, sizeof(workType));
	(*cPacket)->PutData((char*)&SessionID, sizeof(ULONGLONG));

	// 세션 Release
	_MessageQ->Enqueue(cPacket);
	SetEvent(_hMessageQueueEvent);
	_pLog._dwUpdateQSize++;
}

void ChatServer::OnRecv(ULONGLONG SessionID, RefCountPointer& cPacket)
{
	RefCountPointer contentsPacket = RefCountPointer::MakeSharedPtr();
	(*contentsPacket)->Initialize(PROTOCOL_MAX_SIZE, 0);
	_pLog._dwPacketPoolUse++;

	WORD workType = en_WORK_PACKET;
	(*contentsPacket)->PutData((char*)&workType, sizeof(workType));
	(*contentsPacket)->PutData((char*)&SessionID, sizeof(SessionID));
	(*contentsPacket)->PutData((char*)(*cPacket)->GetHeadPtr(), (*cPacket)->GetDataSize());

	cPacket.DecRefCount();
	_pLog._dwPacketPoolUse--;

	_MessageQ->Enqueue(contentsPacket);
	SetEvent(_hMessageQueueEvent);
	_pLog._dwUpdateQSize++;
}

void ChatServer::OnError(int errorcode, WCHAR* message)
{

}

unsigned int WINAPI ChatServer::ContentsThread(LPVOID arg)
{
	ChatServer* thisPtr = (ChatServer*)arg;

	HANDLE hHandleArr[3] = { thisPtr->_hQuitEvent, thisPtr->_hMessageQueueEvent, thisPtr->_hTimeoutEvent};

	// 컨텐츠 스레드 로그 등록
	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	DWORD nowTime;
	DWORD ret = 0;
	while (1)
	{
		_pLog._dwUpdateTPS++;

		DWORD sleepTime = dfSLEEPTIME;

		DWORD checkTime;
		//checkTime = timeGetTime();
		// 메세지 큐에서 Dequeue후 작업
		thisPtr->MessageProc();
		//printf("\n\n-------ProcTime : %d--------\n\n", timeGetTime() - checkTime);

		thisPtr->TimeCheck(sleepTime);

		// 프레임은 없어야 한다. 일이 있을때만 꺠어나서, 메세지를 처리해야 한다.
		nowTime = timeGetTime();

		ret = WaitForMultipleObjects(3, hHandleArr, FALSE, dfSLEEPTIME);
		_pLog._dwUpdateThreadSleepTime += (timeGetTime() - nowTime);

		if (ret == WAIT_OBJECT_0)
		{
			// 서버 종료
			return 0;
		}
	}
}