#pragma once
#pragma comment(lib,"Pdh.lib")

#include <stdio.h>
#include <Pdh.h>
#include <pdhmsg.h>

class PDHMonitor
{
public:
	PDHMonitor()
	{
		// PDH 쿼리 핸들 생성
		PdhOpenQuery(NULL, NULL, &_CpuQuery);

		// PDH 리소스 카운터 생성 (여러개 수집시 이를 여러개 생성)
		PdhAddCounter(_CpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &_CpuTotal);
		PdhAddCounter(_CpuQuery, L"\\Memory\\Pool Nonpaged Bytes", NULL, &_NonpagedMem);
		PdhAddCounter(_CpuQuery, L"\\Memory\\Available MBytes", NULL, &_AvailableMem);
		PdhAddCounter(_CpuQuery, L"\\Process(IOCP_ChatServer_MultiThread)\\Private Bytes", NULL, &_PrivateMemoryUsage);
		PdhAddCounter(_CpuQuery, L"\\Network Interface(*)\\Bytes Received/sec", NULL, &_NetworkRecvSec);
		PdhAddCounter(_CpuQuery, L"\\Network Interface(*)\\Bytes Sent/sec", NULL, &_NetworkSentSec);

		// 첫 갱신
		PdhCollectQueryData(_CpuQuery);
	}

	void QueryUpdate()
	{
		PdhCollectQueryData(_CpuQuery);
	}

	LONG GetNonpagedMem()
	{
		PDH_FMT_COUNTERVALUE npmVal;
		PdhGetFormattedCounterValue(_NonpagedMem, PDH_FMT_LONG, NULL, &npmVal);

		return npmVal.longValue;
	}

	LONG GetAvailableMem()
	{
		PDH_FMT_COUNTERVALUE ableMeVal;
		PdhGetFormattedCounterValue(_AvailableMem, PDH_FMT_LONG, NULL, &ableMeVal);

		return ableMeVal.longValue;
	}
	
	LONG GetCPUTotalUsage()
	{
		/*PDH_FMT_COUNTERVALUE cpuVal;
		PdhGetFormattedCounterValue(_CpuTotal, PDH_FMT_LONG, NULL, &cpuVal);*/

		LONG returnValue = SumCounterArrayLong(_CpuTotal);

		return returnValue;
	}

	LONG GetPrivateMemory()
	{
		PDH_FMT_COUNTERVALUE privateMemVal;
		PdhGetFormattedCounterValue(_PrivateMemoryUsage, PDH_FMT_LONG, NULL, &privateMemVal);

		return privateMemVal.longValue;
	}

	DWORD GetNetworkRecv()
	{
		/*
		PDH_STATUS Status;
		PDH_FMT_COUNTERVALUE networkRecv;
		Status = PdhGetFormattedCounterValue(_NetworkRecvSec, PDH_FMT_LONG, NULL, &networkRecv);
		if (Status != ERROR_SUCCESS)
			DebugBreak();
		*/

		LONG returnValue = SumCounterArrayLong(_NetworkRecvSec);

		return returnValue;
	}

	DWORD GetNetworkSent()
	{
		/*
		PDH_STATUS Status;
		PDH_FMT_COUNTERVALUE networkSent;
		Status = PdhGetFormattedCounterValue(_NetworkSentSec, PDH_FMT_LONG, NULL, &networkSent);
		if (Status != ERROR_SUCCESS)
			DebugBreak();
		*/

		LONG returnValue = SumCounterArrayLong(_NetworkSentSec);

		return returnValue;
	}

	// 네트워크 모든 이더넷의 사용량을 얻어오기 위한 함수. 다른거에도 사용 가능할듯
	// https://learn.microsoft.com/ko-kr/windows/win32/api/pdh/nf-pdh-pdhgetformattedcounterarraya
	LONG SumCounterArrayLong(PDH_HCOUNTER hCtr) {
		DWORD bufSize = 0;
		DWORD itemCount = 0;

		// 처음엔 PDH_MORE_DATA를 무조건 반환하고, bufSize에 버퍼 사이즈를 제공해줌.
		// 그래서 처음에 bufSize를 0으로 해서 1차 호출 -> 버퍼 사이즈와 itemCount 얻어옴
		PDH_STATUS st = PdhGetFormattedCounterArray(hCtr, PDH_FMT_LONG,
			&bufSize, &itemCount, nullptr);
		if (st != PDH_MORE_DATA && st != ERROR_SUCCESS) 
			return 0;
		if (bufSize == 0 || itemCount == 0) 
			return 0;

		std::vector<BYTE> buf(bufSize);
		auto items = reinterpret_cast<PPDH_FMT_COUNTERVALUE_ITEM>(buf.data());

		// 여기서 실질적으로 값을 얻어와서 items에 저장함.
		PDH_STATUS st2 = PdhGetFormattedCounterArray(hCtr, PDH_FMT_LONG, &bufSize, &itemCount, items);
		if (st2 != ERROR_SUCCESS)
			return 0;

		LONG sum = 0;
		for (DWORD i = 0; i < itemCount; ++i) {
			if (items[i].FmtValue.CStatus == ERROR_SUCCESS)
				sum += items[i].FmtValue.longValue; // Bytes/sec (LONG)
		}
		return sum;
	}

	
private:
	PDH_HQUERY _CpuQuery;
	PDH_HCOUNTER _CpuTotal;
	PDH_HCOUNTER _NonpagedMem;
	PDH_HCOUNTER _AvailableMem;
	PDH_HCOUNTER _PrivateMemoryUsage;
	PDH_HCOUNTER _NetworkRecvSec;
	PDH_HCOUNTER _NetworkSentSec;
};