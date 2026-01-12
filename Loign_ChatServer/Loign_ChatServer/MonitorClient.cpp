#include "Includes.h"
#include "CommonProtocol_Login.h"
#include "LanClient.h"
#include "MonitorClient.h"

void MonitorClient::InitMonitorClient()
{
	Init();

	Connect();
}

void MonitorClient::SendMonitorData(BYTE dataType, int dataValue, int timeStamp)
{
	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	(*cPacket)->Initialize(sizeof(st_LanHeader));

	mpREQMonitorDataUpdate(cPacket, dataType, dataValue, timeStamp);
	SendPacket_UniCast(cPacket);

	return;
}

bool MonitorClient::OnConnect()
{
	// 로그인을 보내야한다.
	RefCountPointer cPacket = RefCountPointer::MakeSharedPtr();
	(*cPacket)->Initialize(sizeof(st_LanHeader));
	mpREQMonitorLogin(cPacket);

	SendPacket_UniCast(cPacket);

	//RefCountPointer dataPacket = RefCountPointer::MakeSharedPtr();
	//(*dataPacket)->Initialize(sizeof(st_LanHeader));
	//mpREQMonitorDataUpdate(dataPacket, dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN, true, timeGetTime());

	//// 다 바꾸고, 보내기

	//SendPacket_UniCast(dataPacket);
	return true;
}

bool MonitorClient::OnRecv(RefCountPointer& cPacket)
{
	return true;
}

void MonitorClient::mpREQMonitorLogin(RefCountPointer& cPacket)
{
	en_PACKET_TYPE packetType = en_PACKET_SS_MONITOR_LOGIN;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << LOGINSERVERNUM;
}

void MonitorClient::mpREQMonitorDataUpdate(RefCountPointer& cPacket, BYTE dataType, int dataValue, int timeStamp)
{
	en_PACKET_TYPE packetType = en_PACKET_SS_MONITOR_DATA_UPDATE;

	(**cPacket) << (WORD)packetType;
	(**cPacket) << dataType;
	(**cPacket) << dataValue;
	(**cPacket) << timeStamp;
}