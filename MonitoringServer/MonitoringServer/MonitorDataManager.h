#pragma once
#include "LogManager.h"
#include "PDHMonitor.h"
#include "CPUUsage.h"
#include "DBConnector.h"
#include "DBWriter.h"
// 옵저버 패턴을 이용해서 모니터링 클라 서버에는 옵저버 패턴으로 정보 전달
// 채팅 서버 모니터링 서버는 거기서 데이터를 받아와야 함.

// 모니터링해서 저장해둘 값들
struct MonitorValue
{
	int CurrentValue;
	int TotalValue;
	int MaxValue;
	int MinValue;
	int CountTime;
};

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

		_DBWriterManager = new SHS::DBWriterManager();
		_DBWriterManager->InitDBWriterManager(1);

		_hMonitorThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		_hMonitorThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, this, 0, &_iMonitorThreadID);
		if (_hMonitorThreadHandle == NULL)
			DebugBreak();

		// 서버 컴퓨터 상태 측정용 맵 추가
		MonitorValue* dataArr = new MonitorValue[45];
		memset(dataArr, 0, sizeof(MonitorValue) * 45);
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
				if (((*it).second)[i].CurrentValue != 0)
				{
					pMonitorClientServer->Update((*it).first, i, ((*it).second)[i].CurrentValue, timeStamp, &_pLog);
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
			MonitorValue* dataArr = new MonitorValue[45];
			memset(dataArr, 0, sizeof(MonitorValue) * 45);
			_MonitorDataMap.insert({ serverNum, dataArr });

			it = _MonitorDataMap.find(serverNum);
		}

		// 첫 갱신일 때 Min이 0이되지 않도록
		if (((*it).second)[dataType].CountTime == 0)
		{
			((*it).second)[dataType].CurrentValue = dataValue;
			((*it).second)[dataType].TotalValue += dataValue;
			((*it).second)[dataType].MinValue = dataValue;
			((*it).second)[dataType].MaxValue = dataValue;
		}
		else
		{
			((*it).second)[dataType].CurrentValue = dataValue;
			((*it).second)[dataType].TotalValue += dataValue;
			((*it).second)[dataType].MinValue = min(dataValue, ((*it).second)[dataType].MinValue);
			((*it).second)[dataType].MaxValue = max(dataValue, ((*it).second)[dataType].MaxValue);
		}
		((*it).second)[dataType].CountTime++;
		

		//pMonitorClientServer->Update(serverNum, dataType, dataValue, timeStamp);
	}
private:
	void CreateMonthlyTable()
	{
		ostringstream oss;

		LPVOID pJob = _DBWriterManager->AllocJobAddress();
		CDBCreateMonitorTable* pCDBTable = new(pJob) CDBCreateMonitorTable;

		pCDBTable->_NowTime = _CurrentYM;
		_DBWriterManager->EnqueueJob(pCDBTable, -1);
	}

	// DB 저장 함수
	void DBUpdate()
	{
		bool bCreated = false;

		time_t curtime = time(NULL);
		tm nowTime;
		int err = localtime_s(&nowTime, &curtime);
		if (err != 0)
		{
			printf("DBUpdate - localtime Error : %d\n", err);
			DebugBreak();
			return;
		}

		if (_CurrentYM.tm_year != nowTime.tm_year || _CurrentYM.tm_mon != nowTime.tm_mon)
		{
			_CurrentYM = nowTime;
			CreateMonthlyTable();
		}

		for (auto it = _MonitorDataMap.begin(); it != _MonitorDataMap.end(); it++)
		{
			int timeStamp = (int)time(NULL);
			// 모든 서버의 데이터를 순회
			for (int i = 1; i <= 44; i++)
			{
				// 측정하지 않은 데이터는 DB저장도 하지말자?
				if (((*it).second[i]).CountTime == 0)
					continue;

				// 메모리를 받아서 placementNew
				LPVOID pJob = _DBWriterManager->AllocJobAddress();
				CDBMonitorInsert* pCDBMonitor = new(pJob) CDBMonitorInsert;

				// 연도+월을 테이블 이름으로 설정함				
				ostringstream oss;
				int month = nowTime.tm_mon + 1;
				if(month < 10)
					oss << "monitorlog_" << nowTime.tm_year + 1900 << "0" << month;
				else
					oss << "monitorlog_" << nowTime.tm_year + 1900 << month;

				pCDBMonitor->_ServerNum = (*it).first;
				pCDBMonitor->_NowTime = nowTime;
				pCDBMonitor->_MonitorType = i;
				strncpy_s(pCDBMonitor->_TableName, oss.str().c_str(), sizeof(pCDBMonitor->_TableName) - 1);
				pCDBMonitor->_Min = (*it).second[i].MinValue;
				pCDBMonitor->_Max = (*it).second[i].MaxValue;
				pCDBMonitor->_Avr = ((*it).second[i].TotalValue / (*it).second[i].CountTime);

				// Total, CountTime은 초기화해서 평균 다시낼 것
				// @@TODO : Min이 0을 찍는게 모든거에 나옴... 측정하지 않은 경우의 0은 빼야한다.
				(*it).second[i].TotalValue = 0;
				(*it).second[i].CountTime = 0;

				_DBWriterManager->EnqueueJob(pCDBMonitor, -1);
			}
		}
	}

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
		LogController::GetInstance()->RegisterLogStruct(&_pLog);
		int cnt = 0;

		while (1)
		{
			WaitForSingleObject(thisPtr->_hMonitorThreadEvent, 1000);

			thisPtr->UpdateTotalData();

			thisPtr->UpdateAllData();

			cnt++;
			if (cnt == 60)
			{
				thisPtr->DBUpdate();
				cnt = 0;
			}
		}

		return 0;
	}

	static thread_local stChatLog _pLog;

	tm _CurrentYM;
	CCpuUsage* _pCpuUsage;
	PDHMonitor* _pPDHMonitor;
	MonitorClientServer* pMonitorClientServer;
	SHS::DBWriterManager* _DBWriterManager;

	// 스레드 핸들, ID
	HANDLE _hMonitorThreadEvent;
	HANDLE _hMonitorThreadHandle;
	unsigned int _iMonitorThreadID;

	// serverNum, dataArr[45] -> 서버번호와 데이터를 배열로 저장
	unordered_map<int, MonitorValue*> _MonitorDataMap;
};