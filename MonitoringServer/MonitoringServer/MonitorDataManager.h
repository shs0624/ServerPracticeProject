#pragma once
#include "PDHMonitor.h"
#include "CPUUsage.h"
// 옵저버 패턴을 이용해서 모니터링 클라 서버에는 옵저버 패턴으로 정보 전달
// 채팅 서버 모니터링 서버는 거기서 데이터를 받아와야 함.

class DataObserver
{
public:
	virtual void Update(BYTE serverNum, BYTE dataType, int dataValue, int timeStamp) = 0;
};

class DataSubject
{
public:
	virtual void RegisterObserver(DataObserver* observer) = 0;
	virtual void RemoveObserver(DataObserver* observer) = 0;
	virtual void NotifyObserver(BYTE serverNum, BYTE dataType, int dataValue, int timestamp) = 0;
};

class MonitorDataManager
{
public:
	void InitDataManager(MonitorClientServer* pClientServer)
	{
		// 정보를 전달할 서버 세팅
		pMonitorClientServer = pClientServer;
		_pPDHMonitor = new PDHMonitor();
		_pCpuUsage = new CCpuUsage();

		_hMonitorThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		_hMonitorThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, this, 0, &_iMonitorThreadID);
		if (_hMonitorThreadHandle == NULL)
			DebugBreak();

		// 서버 컴퓨터 상태 측정용 맵 추가
		int* dataArr = new int[45];
		memset(dataArr, 0, sizeof(int) * 45);
		_MonitorDataMap.insert({ TOTALSERVERNUM, dataArr });
	}

	// 누군가 접속하면, 그냥 접속인원 전체에게 현재 데이터를 전송하자.
	void UpdateAllData()
	{
		// 서버컴퓨터 상태 얻어와서 갱신

		for (auto it = _MonitorDataMap.begin(); it != _MonitorDataMap.end(); it++)
		{
			int timeStamp = (int)time(NULL);
			// 모든 서버의 데이터를 순회
			for (int i = 1; i <= 44; i++)
			{
				// 0이면 안보내면 되나?
				if (((*it).second)[i] != 0)
				{
					pMonitorClientServer->Update((*it).first, i, ((*it).second)[i], timeStamp);
				}
			}
		}
	}

	//
	void UpdateData(BYTE serverNum, BYTE dataType, int dataValue, int timeStamp)
	{
		// 혹시 모를 예외처리
		if (dataType < 1 || dataType > 44)
		{
			DebugBreak();
			return;
		}

		auto it = _MonitorDataMap.find(serverNum);
		if (it == _MonitorDataMap.end())
		{
			int* dataArr = new int[45];
			memset(dataArr, 0, sizeof(int) * 45);
			_MonitorDataMap.insert({ serverNum, dataArr });

			it = _MonitorDataMap.find(serverNum);
		}
		
		((*it).second)[dataType] = dataValue;

		//pMonitorClientServer->Update(serverNum, dataType, dataValue, timeStamp);
	}
private:
	// 1분에 한 번 DB 저장하자.
	//void DBUpdateThread();

	void UpdateTotalData()
	{
		_pCpuUsage->UpdateCpuTime();
		_pPDHMonitor->QueryUpdate();
		int nowTime = (int)time(NULL);

		int cpuTotal = _pCpuUsage->ProcessorTotal(); 
		//int cpuTotal = _pPDHMonitor->GetCPUTotalUsage();
		UpdateData(TOTALSERVERNUM, dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL, cpuTotal, nowTime);

		int nonPagedMemMBytes = _pPDHMonitor->GetNonpagedMem() / 1000000;
		UpdateData(TOTALSERVERNUM, dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, nonPagedMemMBytes, nowTime);

		int availableMem = _pPDHMonitor->GetAvailableMem();
		UpdateData(TOTALSERVERNUM, dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, availableMem, nowTime);

		int netRecvKBytes = _pPDHMonitor->GetNetworkRecv() / 1000;
		UpdateData(TOTALSERVERNUM, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, netRecvKBytes, nowTime);

		int netSentKBytes = _pPDHMonitor->GetNetworkSent() / 1000;
		UpdateData(TOTALSERVERNUM, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, netSentKBytes, nowTime);
	}

	// 시간 측정하며 1초마다 클라에 보내고, 1분마다 DB 저장.
	static unsigned int WINAPI MonitorThread(LPVOID arg)
	{
		MonitorDataManager* thisPtr = (MonitorDataManager*)arg;
		while (1)
		{
			WaitForSingleObject(thisPtr->_hMonitorThreadEvent, 1000);

			thisPtr->UpdateTotalData();

			thisPtr->UpdateAllData();
		}

		return 0;
	}

	CCpuUsage* _pCpuUsage;
	PDHMonitor* _pPDHMonitor;
	MonitorClientServer* pMonitorClientServer;

	// 스레드 핸들, ID
	HANDLE _hMonitorThreadEvent;
	HANDLE _hMonitorThreadHandle;
	unsigned int _iMonitorThreadID;

	// serverNum, dataArr[45] -> 서버번호와 데이터를 배열로 저장
	unordered_map<int, int*> _MonitorDataMap;
};