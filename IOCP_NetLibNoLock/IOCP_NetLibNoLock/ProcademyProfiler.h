#pragma once
#include <Windows.h>

#define PROFILE
#ifdef PROFILE
	#define PRO_BEGIN(TagName) ProfileBegin(TagName)
	#define PRO_END(TagName) ProfileEnd(TagName)
#elif
	#define PRO_BEGIN(TagName)  
	#define PRO_END(TagName)  
#endif

class Profiler
{
public:
	Profiler(const WCHAR* tag);
	~Profiler();
private:
	const WCHAR* tag;
};

void ProfileBegin(const WCHAR* tagName);

void ProfileEnd(const WCHAR* tagName);

void ProfileDataOutText(const CHAR* szFileName);

void ProfileReset(void);