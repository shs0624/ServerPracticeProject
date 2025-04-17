#include "Windows.h"
#include "PacketDefine.h"
#include "CSerializationBuffer.h"
#include "MessageCreate.h"

void mpMoveStart(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y)
{
	header->byCode = 0x89;
	header->byType = dfPACKET_SC_MOVE_START;

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;

	header->bySize = msg->GetDataSize();
}

void mpMoveStop(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y)
{
	header->byCode = 0x89;
	header->byType = dfPACKET_SC_MOVE_STOP;

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;

	header->bySize = msg->GetDataSize();
}

void mpCreateMyCharacter(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y, char HP)
{
	header->byCode = 0x89;
	header->byType = dfPACKET_SC_CREATE_MY_CHARACTER;

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;
	(*msg) << HP;

	header->bySize = msg->GetDataSize();
}

void mpCreateOtherCharacter(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y, char HP)
{
	header->byCode = 0x89;
	header->byType = dfPACKET_SC_CREATE_OTHER_CHARACTER;

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;
	(*msg) << HP;

	header->bySize = msg->GetDataSize();
}

void mpDeleteCharacter(st_PACKET_HEADER* header, CPacket* msg, DWORD id)
{
	header->byCode = 0x89;
	header->byType = dfPACKET_SC_DELETE_CHARACTER;

	(*msg) << id;

	header->bySize = msg->GetDataSize();
}

void mpAttack1(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y)
{
	header->byCode = 0x89;
	header->byType = dfPACKET_SC_ATTACK1;

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;

	header->bySize = msg->GetDataSize();
}

void mpAttack2(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y)
{
	header->byCode = 0x89;
	header->byType = dfPACKET_SC_ATTACK2;

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;

	header->bySize = msg->GetDataSize();
}

void mpAttack3(st_PACKET_HEADER* header, CPacket* msg, DWORD id, char dir, short X, short Y)
{
	header->byCode = 0x89;
	header->byType = dfPACKET_SC_ATTACK3;

	(*msg) << id;
	(*msg) << dir;
	(*msg) << X;
	(*msg) << Y;

	header->bySize = msg->GetDataSize();
}

void mpDamage(st_PACKET_HEADER* header, CPacket* msg, DWORD attackerID, DWORD damagedID, char damage)
{
	header->byCode = 0x89;
	header->byType = dfPACKET_SC_DAMAGE;

	(*msg) << attackerID;
	(*msg) << damagedID;
	(*msg) << damage;

	header->bySize = msg->GetDataSize();
}
