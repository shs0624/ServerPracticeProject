#include "ProcademyProfiler.h"
#include <iostream>
#define STRUCT_ARR_MAX 50

CHAR _Line[200] = "--------------------------------------------------------------------------------------------------\n";
CHAR _Header[200] = "               Name |          Average |             Min |              Max |       Call |\n";

struct Profile_Struct
{
	bool _IsUsing = false;
	bool _IsCounting = false;
	CHAR _Tag[64];
	LARGE_INTEGER _StartTime;
	__int64 _TotalTime;
	__int64 _MinTime;
	__int64 _MaxTime;
	__int64 _CallCount;
};

void ProfileBegin(const CHAR* tagName);

void ProfileEnd(const CHAR* tagName);

void ProfileDataOutText(const CHAR* szFileName);

void ProfileReset(void);

class PrivateProfiler
{
public:
	PrivateProfiler()
	{
		QueryPerformanceFrequency(&_Freq);
	}

	bool FindProfile(int* idx, const CHAR* tag)
	{
		for (int i = 0; i < STRUCT_ARR_MAX; i++)
		{
			if (false == _ProfileArr[i]._IsUsing)
			{
				continue;
			}

			if (!strcmp(_ProfileArr[i]._Tag, tag))
			{
				*idx = i;
				return true;
			}
		}

		return false;
	}

	void AddProfile(int* idx, const CHAR* tag)
	{
		*idx = -1;
		for (int i = 0; i < STRUCT_ARR_MAX; i++)
		{
			if (false == _ProfileArr[i]._IsUsing)
			{
				*idx = i;
				break;
			}
		}

		if (*idx == -1)
		{
			throw -1;
		}

		_ProfileArr[*idx]._IsUsing = true;
		//_ProfileArr[*idx]._StartTime = starTime;
		_ProfileArr[*idx]._MinTime = LLONG_MAX;
		_ProfileArr[*idx]._TotalTime = 0;
		_ProfileArr[*idx]._CallCount = 0;
		strcpy_s(_ProfileArr[*idx]._Tag, 64, tag);
	}

	bool BeginCount(int idx)
	{
		if (true == _ProfileArr[idx]._IsCounting)
		{
			return false;
		}

		_ProfileArr[idx]._IsCounting = true;
		QueryPerformanceCounter(&_ProfileArr[idx]._StartTime);
		return true;
	}

	bool EndCount(int idx, LARGE_INTEGER endTime)
	{
		if (false == _ProfileArr[idx]._IsCounting)
		{
			return false;
		}

		__int64 time = endTime.QuadPart - _ProfileArr[idx]._StartTime.QuadPart;
		_ProfileArr[idx]._TotalTime += time;
		_ProfileArr[idx]._MaxTime = max(_ProfileArr[idx]._MaxTime, time);
		_ProfileArr[idx]._MinTime = min(_ProfileArr[idx]._MinTime, time);
		_ProfileArr[idx]._CallCount++;
		_ProfileArr[idx]._IsCounting = false;

		return true;
	}

	void WriteFile(FILE* file)
	{
		for (int i = 0; i < STRUCT_ARR_MAX; i++)
		{
			if (false == _ProfileArr[i]._IsUsing)
			{
				continue;
			}

			// 사용중인 배열이면, 계산 후 받은 파일에 정보 fwrite
			// 1 마이크로 세컨드 = 100만분의 1초, 1초 = 100만 마이크로 세컨드, 0.1초 = 10만 마이크로 세컨드
			//  Freq로 나눠서 초단위로 변환하고, 100만을 곱해주면 그게 마이크로 세컨드.
			CHAR context[200];
			double average = _ProfileArr[i]._TotalTime - (_ProfileArr[i]._MaxTime + _ProfileArr[i]._MinTime);
			average = ((average / (_ProfileArr[i]._CallCount - 2))) * (1000000.0f / (float)_Freq.QuadPart);
			double min = (double)((double)_ProfileArr[i]._MinTime) * (1000000.0f / (float)_Freq.QuadPart);
			double max = (double)((double)_ProfileArr[i]._MaxTime) * (1000000.0f / (float)_Freq.QuadPart);
			sprintf_s(context, 200, "%20s | %.4f㎲ | %.4f㎲ | %.4f㎲ | %lld\n",
				_ProfileArr[i]._Tag, average, min, max, _ProfileArr[i]._CallCount);
			fwrite(&context, strlen(context), 1, file);
		}
	}

	void ResetProfiles()
	{
		for (int i = 0; i < STRUCT_ARR_MAX; i++)
		{
			if (false == _ProfileArr[i]._IsUsing)
			{
				continue;
			}

			_ProfileArr[i]._TotalTime = 0;
			_ProfileArr[i]._MaxTime = 0;
			_ProfileArr[i]._MinTime = LLONG_MAX;
			_ProfileArr[i]._CallCount = 0;
		}
	}

private:
	Profile_Struct _ProfileArr[STRUCT_ARR_MAX];
	LARGE_INTEGER _Freq;
};

PrivateProfiler _Profiler;

Profiler::Profiler(const char* tag)
{
	PRO_BEGIN(tag);
	this->tag = tag;
}

Profiler::~Profiler()
{
	PRO_END(tag);
}

void ProfileBegin(const CHAR* tagName)
{
	int idx;
	LARGE_INTEGER startTime;

	QueryPerformanceCounter(&startTime);
	if (false == _Profiler.FindProfile(&idx, tagName))
	{
		//구조체 추가
		_Profiler.AddProfile(&idx, tagName);
	}

	// Begin-Begin 구조인지 확인
	if (false == _Profiler.BeginCount(idx))
	{
		// Begin-Begin구조면 크래쉬
		throw 1;
	}
}

void ProfileEnd(const CHAR* tagName)
{
	int idx;
	LARGE_INTEGER endTime;

	QueryPerformanceCounter(&endTime);
	if (false == _Profiler.FindProfile(&idx, tagName))
	{
		// 없는 태그를 End했음. 이걸 알려야 할까?
		DebugBreak();
		return;
	}

	if (false == _Profiler.EndCount(idx, endTime))
	{
		// End-End 구조면 크래쉬
		throw 1;
	}
}

void ProfileDataOutText(const CHAR* szFileName)
{
	FILE* fptr;
	fopen_s(&fptr, szFileName, "wb");
	if (fptr == nullptr)
	{
		throw 0;
	}

	fwrite(_Line, strlen(_Line), 1, fptr);
	fwrite(_Header, strlen(_Header), 1, fptr);
	fwrite(_Line, strlen(_Line), 1, fptr);
	_Profiler.WriteFile(fptr);
	fwrite(_Line, strlen(_Line), 1, fptr);

	fclose(fptr);
}

void ProfileReset(void)
{
	_Profiler.ResetProfiles();
}