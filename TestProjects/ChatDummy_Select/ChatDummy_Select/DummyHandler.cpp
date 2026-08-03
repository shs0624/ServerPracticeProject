#pragma once
#include "Includes.h"
#include "PacketDefine.h"
#include "CommonProtocol.h"
#include "LogController.h"
#include "ChatDummy.h"
#include "DummyHandler.h"
#include "ChatDummyController.h"
#include "TCPNetwork.h"

void DummyHandler::InitHandler(SOCKADDR_IN serverAddr, int sessionCount, int startIdx, bool IsTestTimeout, bool bTestFlood)
{
	_DummyController = new ChatDummyController(this);
	_NetworkController = new TCPNetworkController(serverAddr, this);

	_NetworkController->netStartUp(sessionCount, startIdx, IsTestTimeout);
	_DummyController->InitController(sessionCount, startIdx, IsTestTimeout, bTestFlood);
}

void DummyHandler::Update()
{
	_NetworkController->netSelectIO();

	_DummyController->Update();
}

void DummyHandler::RequestSendPacket(DWORD sessionID, RefCountPointer& refCountPointer, int repeat)
{
	_NetworkController->SendPacket(sessionID, refCountPointer, repeat);
}

void DummyHandler::RequestConnect(DWORD sessionID)
{
	_NetworkController->Connect(sessionID);
} 

void DummyHandler::RequestDisconnect(DWORD sessionID)
{
	_NetworkController->DisconnectSession(sessionID);
}

// L4 -> L7
void DummyHandler::OnConnected(DWORD sessionID)
{
	_DummyController->OnConnect(sessionID);
}

void DummyHandler::OnRecv(DWORD sessionID, RefCountPointer& refCountPointer)
{
	// 더미 컨트롤러에게 메시지 넘기기
	_DummyController->OnRecv(sessionID, refCountPointer);
}

void DummyHandler::OnDisconnect(DWORD sessionID)
{
	_DummyController->OnDisconnect(sessionID);
}