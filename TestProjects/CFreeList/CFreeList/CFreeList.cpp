//#define __TEST__
#include "CFreeList.h"
#include "ProcademyProfiler.h"
#include <iostream>

struct session
{
	int ID;
	int level;
	SOCKADDR_IN* addr;
	char name[50];
};

class Test
{
public:
	Test()
	{
		num = 63;
	}

	~Test()
	{
		num = 0;
	}

	int num;
	session _session[10];
};

int main()
{
	procademy::CMemoryPool<Test> memPool(500, true, false);
	Test* ptrarr[501];
	int cnt = 0;
	LARGE_INTEGER start;
	LARGE_INTEGER end;

	{
		Test* test;

		QueryPerformanceCounter(&start);
		for (int i = 0; i < 500; i++)
		{
			Profiler("memPool alloc");
			test = memPool.Alloc();
			ptrarr[i] = test;
		}

		for (int i = 0; i < 150; i++)
		{
			Profiler("memPool free");
			memPool.Free(ptrarr[i]);
		}
		QueryPerformanceCounter(&end);

		printf("memPool Alloc Free : %lld\n", end.QuadPart - start.QuadPart);
	}

	{
		Test* session;

		QueryPerformanceCounter(&start);
		for (int i = 0; i < 300; i++)
		{
			Profiler("new");
			session = new Test();
			ptrarr[i] = session;
		}

		for (int i = 0; i < 300; i++)
		{
			Profiler("delete");
			delete(ptrarr[i]);
		}
		QueryPerformanceCounter(&end);

		printf("new/delete Alloc Free : %lld\n", end.QuadPart - start.QuadPart);
	}

	ProfileDataOutText("TimeText.txt");
	return 0;
}