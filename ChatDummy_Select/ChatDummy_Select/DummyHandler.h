#pragma once
class ChatDummyController;
class TCPNetworkController;

class DummyHandler
{
public:
	void InitHandler(SOCKADDR_IN serverAddr, int sessionCount, int startIdx, bool IsTestTimeout);
	void Update();

	// L7 -> L4
	void RequestSendPacket(DWORD sessionID, RefCountPointer& refCountPointer);
	void RequestConnect(DWORD sessionID);
	void RequestDisconnect(DWORD sessionID);

	// L4 -> L7
	void OnConnected(DWORD sessionID);
	void OnRecv(DWORD sessionID, RefCountPointer& refCountPointer);
	void OnDisconnect(DWORD sessionID);

private:
	ChatDummyController* _DummyController;
	TCPNetworkController* _NetworkController;
};