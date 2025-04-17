#pragma once

struct st_CHARACTER
{
	st_SESSION* pSession;
	DWORD dwSessionID;

	DWORD dwAction;
	BYTE byDirection;
	BYTE byMoveDirection;

	short shX;
	short shY;

	st_SECTOR_POS CurSector;
	st_SECTOR_POS OldSector;

	char chHP;
};

//-----------------------------------------------------------------
// 30초 이상이 되도록 아무런 메시지 수신도 없는경우 접속 끊음.
//-----------------------------------------------------------------
#define dfNETWORK_PACKET_RECV_TIMEOUT	30000


//-----------------------------------------------------------------
// 화면 이동 범위.
//-----------------------------------------------------------------
#define dfRANGE_MOVE_TOP	0
#define dfRANGE_MOVE_LEFT	0
#define dfRANGE_MOVE_RIGHT	6400
#define dfRANGE_MOVE_BOTTOM	6400

//-----------------------------------------------------------------
// 섹터 크기.
//-----------------------------------------------------------------
#define dfSECTOR_SIZE_Y 100
#define dfSECTOR_SIZE_X 100
#define dfSECTOR_MAX_Y 64
#define dfSECTOR_MAX_X 64

//---------------------------------------------------------------
// 공격범위.
//---------------------------------------------------------------
#define dfATTACK1_RANGE_X		80
#define dfATTACK2_RANGE_X		90
#define dfATTACK3_RANGE_X		100
#define dfATTACK1_RANGE_Y		10
#define dfATTACK2_RANGE_Y		10
#define dfATTACK3_RANGE_Y		20


//---------------------------------------------------------------
// 공격 데미지.
//---------------------------------------------------------------
#define dfATTACK1_DAMAGE		1
#define dfATTACK2_DAMAGE		2
#define dfATTACK3_DAMAGE		3


//-----------------------------------------------------------------
// 캐릭터 이동 속도   // 25fps 기준 이동속도
//-----------------------------------------------------------------
#define dfSPEED_PLAYER_X	6	// 3   50fps
#define dfSPEED_PLAYER_Y	4	// 2   50fps


//-----------------------------------------------------------------
// 이동 오류체크 범위
//-----------------------------------------------------------------
#define dfERROR_RANGE		50

// 캐릭터 체력
#define dfHP_MAX 100