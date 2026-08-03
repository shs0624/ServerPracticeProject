#pragma once
#pragma comment(lib,"Pdh.lib")

#include <stdio.h>
#include <Pdh.h>

class PDHMonitor
{
public:
	PDHMonitor()
	{
		// PDH 쿼리 핸들 생성
		PdhOpenQuery(NULL, NULL, &_CpuQuery);

		// PDH 리소스 카운터 생성 (여러개 수집시 이를 여러개 생성)
		PdhAddCounter(_CpuQuery, L"\\Processor(Loign_ChatServer)\\% Processor Time", NULL, &_CpuTotal);
		PdhAddCounter(_CpuQuery, L"\\Process(Loign_ChatServer)\\Private Bytes", NULL, &_PrivateMemoryUsage);

		// 첫 갱신
		PdhCollectQueryData(_CpuQuery);
	}

	void QueryUpdate()
	{
		PdhCollectQueryData(_CpuQuery);
	}
	
	LONG GetCPUUsage()
	{
		PDH_FMT_COUNTERVALUE cpuVal;
		PdhGetFormattedCounterValue(_CpuTotal, PDH_FMT_LONG, NULL, &cpuVal);

		return cpuVal.longValue;
	}

	LONG GetPrivateMemory()
	{
		PDH_FMT_COUNTERVALUE privateMemVal;
		PdhGetFormattedCounterValue(_PrivateMemoryUsage, PDH_FMT_LONG, NULL, &privateMemVal);

		return privateMemVal.longValue;
	}
private:
	PDH_HQUERY _CpuQuery;
	PDH_HCOUNTER _CpuTotal;
	PDH_HCOUNTER _PrivateMemoryUsage;
};