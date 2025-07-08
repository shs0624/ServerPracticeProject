#include "Windows.h"
#include "PacketDefine.h"
#include "CSerializationBuffer.h"
#include "MessageCreate.h"
#include "LogProc.h"

extern int g_iLogLevel;
extern WCHAR g_szLogBuff[1024];


void mpMoveStart(CPacket* msg, DWORD id, char dir, short X, short Y)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_MOVE_START # sessionID : %d\n", id);
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 9;
	header.byType = dfPACKET_SC_MOVE_START;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;
}

void mpMoveStop(CPacket* msg, DWORD id, char dir, short X, short Y)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_MOVE_STOP # sessionID : %d\n", id);
	st_PACKET_HEADER header;

	header.byCode = 0x89;	
	header.bySize = 9;
	header.byType = dfPACKET_SC_MOVE_STOP;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;
}

void mpSync(CPacket* msg, DWORD id, short X, short Y)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_SYNC # sessionID : %d\n", id);
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 8;
	header.byType = dfPACKET_SC_SYNC;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << id;
	(*msg) << X;
	(*msg) << Y;
}

void mpCreateMyCharacter(CPacket* msg, DWORD id, char dir, short X, short Y, char HP)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_CREATE_MY_CHARACTER # sessionID : %d # X : %d # Y : %d\n", id, X, Y);
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 10;
	header.byType = dfPACKET_SC_CREATE_MY_CHARACTER;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;
	(*msg) << HP;
}

void mpCreateOtherCharacter(CPacket* msg, DWORD id, char dir, short X, short Y, char HP)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_CREATE_OTHER_CHARACTER # sessionID : %d # X : %d # Y : %d\n", id, X, Y);
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 10;
	header.byType = dfPACKET_SC_CREATE_OTHER_CHARACTER;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;
	(*msg) << HP;
}

void mpDeleteCharacter(CPacket* msg, DWORD id)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_DELETE_CHARACTER # sessionID : %d\n", id);
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 4;
	header.byType = dfPACKET_SC_DELETE_CHARACTER;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << id;
}

void mpAttack1(CPacket* msg, DWORD id, char dir, short X, short Y)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_ATTACK1 # sessionID : %d\n", id);
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 9;
	header.byType = dfPACKET_SC_ATTACK1;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;
}

void mpAttack2(CPacket* msg, DWORD id, char dir, short X, short Y)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_ATTACK2 # sessionID : %d\n", id);
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 9;
	header.byType = dfPACKET_SC_ATTACK2;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;
}

void mpAttack3(CPacket* msg, DWORD id, char dir, short X, short Y)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_ATTACK3 # sessionID : %d\n", id);
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 9;
	header.byType = dfPACKET_SC_ATTACK3;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;
}

void mpDamage(CPacket* msg, DWORD attackerID, DWORD damagedID, char damagedHP)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_DAMAGE # sessionID : %d\n",  attackerID);
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 9;
	header.byType = dfPACKET_SC_DAMAGE;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << attackerID;
	(*msg) << damagedID;
	(*msg) << damagedHP;
}

void mpEcho(CPacket* msg, DWORD time)
{
	_LOG(0, L"Make Packet type : dfPACKET_SC_ECHO\n");
	st_PACKET_HEADER header;

	header.byCode = 0x89;
	header.bySize = 4;
	header.byType = dfPACKET_SC_ECHO;

	msg->PutData((char*)&header, sizeof(st_PACKET_HEADER));

	(*msg) << time;
}
