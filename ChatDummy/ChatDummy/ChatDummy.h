#pragma once
#include "NetClient.h"
#include "CommonProtocol.h"
#include "PacketDefine.h"
#include "ChatDummyManager_MakePacket.h"
#define DISCONNECT_COUNT 100
#define CHAT_COUNT 3

enum DummyType
{
	// 1초마다 이동, 채팅은 3회 이동 마다
	en_Normal,
	en_TimeOut_Session,
	en_TimeOut_User,
	en_Disconnect_Session,
	en_Disconnect_User,
	en_Message_Flood
};

// 할 행동들.
enum DummyAction
{
	en_ActionWait,		// Move,Chat 보내고 결과 기다리기
	en_ActionMove,		// 이동
	en_ActionChat,		// 채팅
	en_ActionLogin,		// 로그인
	en_ActionLoginWait,	// 로그인 결과 기다리기
	en_ActionConnect,	// connect 시도
	en_ActionDisconnect // 연결 끊기
};

class ChatDummy : CNetClient
{
public:
	ChatDummy()
	{
		
	}

	BOOL IsWait() { return _bWait; }

	DummyAction GetNextAction()
	{
		return _enNextAction;
	}

	LPOVERLAPPED GetRecvOverlapped()
	{
		return &(_mySession->recvOverlapped);
	}

	void Init(DummyType type, HANDLE IOCPHandle)
	{
		StartNetClient();

		_enType = type;
		_enNextAction = DummyAction::en_ActionConnect;
		_IOCPHandle = IOCPHandle;
	}

	void Move()
	{
		int nx = rand() % dfSECTOR_MAX_X;
		int ny = rand() % dfSECTOR_MAX_Y;

		_shSectorX = nx;
		_shSectorY = ny;

		RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
		(*cPacket)->Initialize(MAX_PROTOCOLSIZE, sizeof(st_NetHeader));

		mpREQSectorMove(cPacket, _AccountNo, _shSectorX, _shSectorY);

		SendPost(this, cPacket, _IOCPHandle);

		// 다음 행동 정하기
		_shActionCount++;
		UpdateAction();

		_bWait = true;
	}

	void Chat()
	{
		RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
		(*cPacket)->Initialize(MAX_PROTOCOLSIZE, sizeof(st_NetHeader));

		mpREQSectorMove(cPacket, _AccountNo, _shSectorX, _shSectorY);

		SendPost(this, cPacket, _IOCPHandle);

		// 다음 행동 정하기
		_shActionCount++;
		UpdateAction();

		_bWait = true;
	}

	// Wait/Disconnect/LoginWait 상태에서 호출하는 함수. 다음 행동을 설정함.
	virtual void UpdateAction()
	{
		// Disconnect - Connect는 무조건
		if (_enNextAction == en_ActionDisconnect)
		{
			_enNextAction = en_ActionConnect;
			return;
		}

		switch (_enType)
		{
			case DummyType::en_Normal:
			{
				if (_shActionCount >= DISCONNECT_COUNT)
					_enNextAction = en_ActionDisconnect;
				else if (_enNextAction == en_ActionConnect) // @@TODO : 이거 좀 어색한듯
					_enNextAction = en_ActionLogin;
				else if (_shActionCount % CHAT_COUNT == 0)
					_enNextAction = en_ActionChat;
				else
					_enNextAction = en_ActionMove;
				break;
			}
			case DummyType::en_Disconnect_Session:
			{
				// 로그인 대신, 연결 끊기. 로그인 상태가 아니라면 Connect
				if (_enNextAction == en_ActionLogin)
					_enNextAction = en_ActionDisconnect;
				else if(_enNextAction == en_ActionConnect)
					_enNextAction = en_ActionLogin;
				else
					_enNextAction = en_ActionConnect;
				break;
			}
			case DummyType::en_Disconnect_User:
			{
				// 유저가 되면 Disconnect, 유저가 아니라면 Login
				if (_bUser == TRUE)
					_enNextAction = en_ActionDisconnect;
				else if (_enNextAction == en_ActionConnect)
					_enNextAction = en_ActionLogin;
				else
					_enNextAction = en_ActionConnect;
				break;
			}
		}
	}

	bool Login()
	{
		RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
		(*cPacket)->Initialize(MAX_PROTOCOLSIZE, sizeof(st_NetHeader));

		// @@TODO : 로그인 할 유저 정보 픽하기.
		
		// 패킷 생성
		mpREQLogin(cPacket, _AccountNo, _ID, _NickName, _sessionKey);

		_enNextAction = en_ActionLogin;
		// 이렇게 넣으면 완료통지에서 SendQ 확인하고 보낼거임
		return SendPost(this, cPacket, _IOCPHandle);
	}

	void Disconnect()
	{
		closesocket(_mySession->sock);
	}

	bool Connect(SOCKADDR_IN serverAddr)
	{
		_shActionCount = 0;

		return CNetClient::Connect(serverAddr);
	}

	bool OnIOCPRecv_RecvProc(DWORD cbTransferred, DWORD& recvTPS)
	{
		if (!RecvProc_Net(cbTransferred, recvTPS))
		{
			// 같은 작업 한 번 더 시도하게 유도
			_bWait = FALSE;
			return false;
		}

		return true;
	}

	bool SetRecv()
	{
		if (!SetWSARecv())
		{
			return false;
		}

		return true;
	}

	bool OnIOCPSend(DWORD& sendTPS)
	{
		return SendPacket_Re(sendTPS);
	}

	// 패킷을 해체 분석, 틀린 데이터라면 false 반환
	bool PacketProc(RefCountPointer& cPacket)
	{
		en_PACKET_TYPE type;
		(*cPacket)->GetData((char*)&type, sizeof(type));
		
		switch (type)
		{
			case en_PACKET_CS_CHAT_RES_LOGIN:
			{
				LPVOID buffer;
				(*cPacket)->GetData((char*)&buffer, sizeof(BYTE));
				if ((BYTE)buffer != 1)
					return false;
				(*cPacket)->GetData((char*)&buffer, sizeof(INT64));
				if ((INT64)buffer != _AccountNo)
					return false;

				_bUser = TRUE;
				_bWait = FALSE;
				return true;
			}
			case en_PACKET_CS_CHAT_RES_MESSAGE:
			{  
				// 내가 보낸 채팅에 대한 응답이 오면 Wait을 해제. 나머진 패스
				LPVOID buffer;
				(*cPacket)->GetData((char*)&buffer, sizeof(INT64));
				if ((INT64)buffer == _AccountNo)
					_bWait = FALSE;

				return true;
			}
			case en_PACKET_CS_CHAT_RES_SECTOR_MOVE:
			{
				LPVOID buffer;
				(*cPacket)->GetData((char*)&buffer, sizeof(INT64));
				if ((INT64)buffer != _AccountNo)
					return false;
				(*cPacket)->GetData((char*)&buffer, sizeof(WORD));
				if ((WORD)buffer != _shSectorX)
					return false;
				(*cPacket)->GetData((char*)&buffer, sizeof(WORD));
				if ((WORD)buffer != _shSectorY)
					return false;

				_bWait = FALSE;
				return true;
			}
		}

		return false;
	}

	virtual bool OnRecv(RefCountPointer& cPacket)
	{
		// 패킷을 해체하고, 제대로 된 패킷이 왔는지 확인해준다.
		return PacketProc(cPacket);
	}

	virtual bool OnSend()
	{
		return true;
	}
private:
	DWORD _iMessageNotCorrect;

	HANDLE _IOCPHandle;

	DummyAction _enNextAction;
	DummyType _enType;
	short _shSectorX;
	short _shSectorY;
	short _shActionCount;
	short _shChatCount;

	WCHAR _ID[20];
	WCHAR _NickName[20];
	INT64 _AccountNo;
	char _sessionKey[64];

	BOOL _bUser;
	BOOL _bWait;
};