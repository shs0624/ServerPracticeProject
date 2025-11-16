#pragma once
class TCPNetworkController
{
public:
	TCPNetworkController(SOCKADDR_IN serverAddr, DummyHandler* handler)
	{
		_serverAddr = serverAddr;
		_dummyHandler = handler;
	}

	void netStartUp(int sessionCount, int startIdx);

	void netSelectIO();

	void SelectProc(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet);

	bool Connect(DWORD idx);

	void SendPacket(DWORD sessionID, RefCountPointer& cPacket);

	void DisconnectSession(DWORD sessionID);
private:
	void DisconnectSession(st_NetSession* pSession);
	void ConnectProc(st_NetSession* ptr);
	void SendProc(st_NetSession* ptr);

	void netProc_Recv(SOCKET socket);
	void netProc_Send(SOCKET socket);
	void netProc_Except(SOCKET socket);

	void InitSession(int startIdx);

	int _iSessionCount;
	int _iselectIOFrame = 0;

	SOCKADDR_IN _serverAddr;

	DummyHandler* _dummyHandler;
	st_NetSession* _sessionArr[4000];
	unordered_map<SOCKET, st_NetSession*> _sessionMap;
};