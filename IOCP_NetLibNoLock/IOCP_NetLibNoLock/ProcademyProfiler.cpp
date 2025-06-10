#include "ProcademyProfiler.h"
#include <iostream>
#include <unordered_map>
#define STRUCT_ARR_MAX 50
#define THREAD_ARR_MAX 200

CHAR _Line[200] = "--------------------------------------------------------------------------------------------------\n";
CHAR _Header[200] = "               Name |          Average |             Min |              Max |       Call |\n";

struct stProfile_Count
{
	WCHAR _Tag[64];
	LARGE_INTEGER _StartTime;
	bool _IsUsing = false;
};

struct stProfile_Result
{
	WCHAR _Tag[64];
	LARGE_INTEGER _StartTime;
	__int64 _TotalTime;
	__int64 _MinTime;
	__int64 _MaxTime;
	__int64 _CallCount;
};

void ProfileBegin(const WCHAR* tagName);

void ProfileEnd(const WCHAR* tagName);

void ProfileDataOutText(const CHAR* szFileName);

void ProfileReset(void);

class PrivateProfiler
{
public:
	PrivateProfiler()
	{
		QueryPerformanceFrequency(&_Freq);
	}
	 
	bool FindProfile(DWORD threadID, const WCHAR* tag, stProfile_Count** pProfile)
	{
		std::unordered_map<DWORD, stProfile_Count*>::iterator it;
		it = _CountMap.find(threadID);
		if (it != _CountMap.end())
		{
			// threadID는 등록되어 있음
			stProfile_Count* ptr = it->second;
			for (int i = 0; i < STRUCT_ARR_MAX; i++)
			{
				if (wcscmp((ptr + i)->_Tag, tag))
				{
					(*pProfile) = (ptr + i);
					return true;
				}
			}
		}
		else
		{
			// CountMap에 스레드ID는 추가
			stProfile_Count* arr = (stProfile_Count*)malloc(sizeof(stProfile_Count) * THREAD_ARR_MAX);
			_CountMap.insert({ threadID, arr });
			(*pProfile) = arr;
		}

		return false;
	}

	void AddCountProfile(DWORD threadID, stProfile_Count** pProfile, const WCHAR* tag)
	{
		std::unordered_map<DWORD, stProfile_Count*>::iterator it;
		it = _CountMap.find(threadID);
		if (it != _CountMap.end())
		{
			// 있음
			stProfile_Count* ptr = it->second;
			for (int i = 0; i < THREAD_ARR_MAX; i++)
			{
				if ((ptr + i)->_IsUsing == true)
					continue;

				(ptr + i)->_IsUsing = true;
				//(ptr + i)->_StartTime = starTime;
				wcscpy_s((ptr + i)->_Tag, 64, tag);
				*pProfile = (ptr + i);
				break;
			}
		}
		else
		{
			DebugBreak();
		}
	}

	bool BeginCount(DWORD threadID, stProfile_Count* pProfile)
	{
		if (false == pProfile->_IsUsing)
		{
			return false;
		}

		QueryPerformanceCounter(&(pProfile->_StartTime));
		pProfile->_IsUsing = true;
		return true;
	}

	bool EndCount(stProfile_Count* pProfile, LARGE_INTEGER endTime)
	{
		if (false == pProfile->_IsUsing)
		{
			return false;
		}

		SaveCount(pProfile, endTime);
		return true;
	}

	bool SaveCount(stProfile_Count* pProfile, LARGE_INTEGER endTime)
	{
		__int64 time = endTime.QuadPart - pProfile->_StartTime.QuadPart;

		std::unordered_map<WCHAR*, stProfile_Result*>::iterator it;
		it = _ResultMap.find(pProfile->_Tag);

		stProfile_Result* ptr;
		if (it == _ResultMap.end())
		{
			ptr = new stProfile_Result;
			_ResultMap.insert({ pProfile->_Tag, ptr });
		}
		else
		{
			ptr = it->second;
		}

		// 결과를 저장
		ptr->_TotalTime += time;
		ptr->_MaxTime = max(ptr->_MaxTime, time);
		ptr->_MinTime = min(ptr->_MinTime, time);
		ptr->_CallCount++;
		return true;
	}

	void WriteFile(FILE* file)
	{
		stProfile_Result* ptr;
		std::unordered_map<WCHAR*, stProfile_Result*>::iterator it;
		for (it = _ResultMap.begin(); it != _ResultMap.end(); it++)
		{
			ptr = it->second;

			// 1 마이크로 세컨드 = 100만분의 1초, 1초 = 100만 마이크로 세컨드, 0.1초 = 10만 마이크로 세컨드
			//  Freq로 나눠서 초단위로 변환하고, 100만을 곱해주면 그게 마이크로 세컨드.
			CHAR context[200];
			double average = ptr->_TotalTime - (ptr->_MaxTime + ptr->_MinTime);
			average = ((average / (ptr->_CallCount - 2))) * (1000000.0f / (float)_Freq.QuadPart);
			double min = (double)((double)ptr->_MinTime) * (1000000.0f / (float)_Freq.QuadPart);
			double max = (double)((double)ptr->_MaxTime) * (1000000.0f / (float)_Freq.QuadPart);
			sprintf_s(context, 200, "%20s | %.4f㎲ | %.4f㎲ | %.4f㎲ | %lld\n",
				ptr->_Tag, average, min, max, ptr->_CallCount);
			fwrite(&context, strlen(context), 1, file);
		}
	}

	void ResetProfiles()
	{
		std::unordered_map<WCHAR*, stProfile_Result*>::iterator it;

		for (it = _ResultMap.begin(); it != _ResultMap.end(); it++)
		{
			it->second->_TotalTime = 0;
			it->second->_MaxTime = 0;
			it->second->_MinTime = LLONG_MAX;
			it->second->_CallCount = 0;
		}
	}

private:
	// 스레드ID - 카운트용 배열로 매핑된 Map
	std::unordered_map<DWORD, stProfile_Count*> _CountMap;
	// Tag - 결과 구조체로 매핑된 Map
	std::unordered_map<WCHAR*, stProfile_Result*> _ResultMap;
	LARGE_INTEGER _Freq;
};

PrivateProfiler _Profiler;

Profiler::Profiler(const WCHAR* tag)
{
	PRO_BEGIN(tag);
	this->tag = tag;
}

Profiler::~Profiler()
{
	PRO_END(tag);
}

void ProfileBegin(const WCHAR* tagName)
{
	int idx;
	stProfile_Count* pProfile = NULL;

	if (false == _Profiler.FindProfile(GetCurrentThreadId(), tagName, &pProfile))
	{
		//구조체 추가
		_Profiler.AddCountProfile(GetCurrentThreadId(), &pProfile, tagName);
	}

	// Begin-Begin 구조인지 확인
	if (false == _Profiler.BeginCount(GetCurrentThreadId(), pProfile))
	{
		// Begin-Begin구조면 크래쉬
		throw 1;
	}
}

void ProfileEnd(const WCHAR* tagName)
{
	int idx;
	LARGE_INTEGER endTime;
	stProfile_Count* pProfile = NULL;

	QueryPerformanceCounter(&endTime);
	if (false == _Profiler.FindProfile(GetCurrentThreadId(), tagName, &pProfile))
	{
		// 없는 태그를 End했음. 이걸 알려야 할까?
		DebugBreak();
		return;
	}

	if (false == _Profiler.EndCount(pProfile, endTime))
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