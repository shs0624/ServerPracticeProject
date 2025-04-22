#include <Windows.h>
#include "TCPDefine.h"
#include "PacketDefine.h"
#include "TCPNetwork.h"
#include "MessageProc.h"

void ProcessMessage(st_SESSION* session, BYTE type, CPacket* cPacket)
{
	switch (type)
	{
	case dfPACKET_CS_MOVE_START:
		netPacketProc_MoveStart(session, cPacket);
		break;
	case dfPACKET_CS_MOVE_STOP:
		netPacketProc_MoveStop(session, cPacket);
		break;
	case dfPACKET_CS_ATTACK1:
	case dfPACKET_CS_ATTACK2:
	case dfPACKET_CS_ATTACK3:
		netPacketProc_Attack(session, type);
		break;
	case dfPACKET_CS_ECHO:
		netPacketProc_Echo(session);
		break;
	}
}