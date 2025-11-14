#pragma once
#include "NetClient.h"
#include "CommonProtocol.h"
#include "PacketDefine.h"
#include "ChatDummyManager_MakePacket.h"
#define DISCONNECT_COUNT 100
#define CHAT_COUNT 3
#define dfHEARTBEAT_MS 30000
#define dfTIMEOUT_WAIT_MS 5000
#define dfTIMEOUT_USER_MS 40000
#define dfTIMEOUT_SESSION_MS 5000

enum DummyType
{
	// 1초마다 이동, 채팅은 3회 이동 마다
	en_Normal,
	en_TimeOut_Session,
	en_TimeOut_User,
	en_Disconnect_Session,
	en_Disconnect_User,
	// @@TODO : 나중에 추가할 타입
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

	DWORD GetLastHeartbeat()
	{
		return _dwLastHeartBeat;
	}

	void Init(DummyType type, HANDLE IOCPHandle, int id, int nick)
	{
		StartNetClient();

		memcpy(_ID, &id, sizeof(id));
		memcpy(_NickName, &nick, sizeof(nick));

		_enType = type;
		_enNextAction = DummyAction::en_ActionConnect;
		_IOCPHandle = IOCPHandle;

		if (_enType == DummyType::en_TimeOut_Session || DummyType::en_TimeOut_User)
			_bErrorCheckDummy = true;
		else 
			_bErrorCheckDummy = false;
	}

	ERROR_TYPE TimerCheck(DWORD nowTime, DWORD& leftTime)
	{
		if (!_mySession->bConnected)
			return SUCCESS;

		DWORD diff = nowTime - _dwLastMessageTime;
		if (!_bUser)
		{
			// 먼저 로그인 패킷을 보낸 상태인지 확인
			if (_bWait)
			{
				if (diff >= dfTIMEOUT_WAIT_MS)
				{
					// 세션 상태에서 로그인을 보냈지만 응답이 없는 상태
					return TIMEOUT_NOTRECV_LOGIN;
				}

				// 세션 상태에서 로그인을 보냈지만, 응답 타임아웃만큼 기다린건 아닌 상태
				leftTime = dfTIMEOUT_WAIT_MS - diff;
				return SUCCESS;
			}
			else if (diff >= dfTIMEOUT_SESSION_MS)
			{
				// 세션 상태에서 로그인을 안보내고 대기중인 세션이며 타임아웃 시간이 지난 상태
				//Disconnect();
				return NEED_TIMEOUT_SESSION;
			}

			// 세션 상태에서 로그인을 안보내고 대기중인 세션이며 타임아웃까지 남은 상태
			leftTime = dfTIMEOUT_SESSION_MS - diff;
			return SUCCESS;
		}
		else
		{
			if (diff > dfTIMEOUT_USER_MS)
			{
				// 로그인하고, 유저가 타임아웃 시간을 지난 상태
				return NEED_TIMEOUT_USER;
			}

			if (_bWait)
			{
				if (diff >= dfTIMEOUT_WAIT_MS)
				{
					// 로그인하고 메세지를 보냈지만 응답이 시간이 넘게 오지 않은 상태
					return TIMEOUT_NOTRECV;
				}

				// 로그인하고 메세지를 보내고 기다리고 있는 상태
				leftTime = dfTIMEOUT_WAIT_MS - diff;
				return SUCCESS;
			}

			// 로그인하고 메세지는 안보냈고 타임아웃 시간도 안 된 상태
			leftTime = dfTIMEOUT_USER_MS - diff;
			return SUCCESS;
		}
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

	bool HeartBeat(DWORD nowTime)
	{
		if (!_bUser)
			return false;

		if (_enType == en_TimeOut_Session || _enType == en_TimeOut_User)
			return false;

		if (nowTime - _dwLastHeartBeat < dfHEARTBEAT_MS)
			return false;

		RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
		(*cPacket)->Initialize(MAX_PROTOCOLSIZE, sizeof(st_NetHeader));

		mpREQHeartBeat(cPacket);

		SendPost(this, cPacket, _IOCPHandle);

		_dwLastHeartBeat = timeGetTime();

		return true;
		// 다음 행동 정하기
		/*_shActionCount++;
		UpdateAction();*/

		//_bWait = true;
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

		_bWait = TRUE;
		// 이렇게 넣으면 완료통지에서 SendQ 확인하고 보낼거임
		return SendPost(this, cPacket, _IOCPHandle);
	}

	void Disconnect()
	{
		closesocket(_mySession->sock);

		_mySession->bConnected = false;
	}

	bool Connect(SOCKADDR_IN serverAddr)
	{
		_shActionCount = 0;
		
		// ID, NIck은 초기화 필요없음.세션키만 다시 설정
		SetSessionKey();

		// @@TODO : 접속직후 하트비트를 해야하나?
		_dwLastHeartBeat = timeGetTime();

		return CNetClient::Connect(serverAddr);
	}

	void SetSessionKey()
	{
		unsigned char K = rand();
		unsigned char RK = rand();
		unsigned char* cursorPtr = (unsigned char*)_sessionKey;
		unsigned char* tailPtr = (unsigned char*)_sessionKey + 64;

		unsigned char E = 0;
		unsigned char P = 0;

		int cnt = 1;
		while (cursorPtr != tailPtr)
		{
			unsigned char D = *cursorPtr;

			P = D ^ (P + RK + cnt);
			E = P ^ (E + K + cnt);

			*cursorPtr = E;

			cursorPtr++;
			cnt++;
		}

		int a = 3;
	}

	bool OnIOCPRecv_RecvProc(DWORD cbTransferred, DWORD& recvTPS)
	{
		if (!RecvProc_Net(cbTransferred, recvTPS))
		{
			// 같은 작업 한 번 더 시도하게 유도
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
		_dwLastMessageTime = timeGetTime();
		_bWait = TRUE;

		return true;
	}
private:
	DWORD _iMessageNotCorrect;
	DWORD _dwLastHeartBeat;
	DWORD _dwLastMessageTime;

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

	BOOL _bErrorCheckDummy;
	BOOL _bUser;
	BOOL _bWait;
};