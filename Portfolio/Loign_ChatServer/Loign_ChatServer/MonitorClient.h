#pragma once

class MonitorClient : CLanClient
{
public:
	MonitorClient() {}

	void InitMonitorClient();

	bool OnConnect();

	bool OnRecv(RefCountPointer& cPacket);

	void SendMonitorData(BYTE dataType, int dataValue, int timeStamp);
private:
	void mpREQMonitorLogin(RefCountPointer& cPacket);

	void mpREQMonitorDataUpdate(RefCountPointer& cPacket, BYTE dataType, int dataValue, int timeStamp);
};