#include "ProcademyProfiler.h"
#include "RefCountPointer.h"
#include <iostream>
#include <Windows.h>
#include <time.h>
#include <list>
#include "CStack.h"
using namespace std;

struct st_Test
{
	ULONGLONG _sessionID;
	int* ptr;
	int* ptr2;

	short x;
	short y;

	bool _bFlag;
};

st_Test* testArr[1000];

int main()
{
	list<st_Test*>::iterator it;
	list<st_Test*> list;
	st_Test* arr[1000];

	for (int i = 0; i < 100; i++)
	{
		testArr[i] = (st_Test*)new st_Test;
	}

	LARGE_INTEGER startTime, endTime;
	{
		for (int i = 0; i < 100; i++)
		{
			list.push_back(testArr[i]);
		}
		QueryPerformanceCounter(&startTime);
		for (it = list.begin(); it != list.end(); it++)
		{
			(*it)->x = 0;
		}
		QueryPerformanceCounter(&endTime);
	}

	printf("\n\nlist : %lld \n", endTime.QuadPart - startTime.QuadPart);

	{
		//Profiler("Stack");
		int cnt = 0;
		for (int i = 0; i < 100; i++)
		{
			arr[cnt++] = testArr[i];
		}

		QueryPerformanceCounter(&startTime);
		for (int i = 0; i < 100; i++)
		{
			arr[i]->x = 0;
		}
		QueryPerformanceCounter(&endTime);
	}

	printf("\n\nstack : %lld \n", endTime.QuadPart - startTime.QuadPart);

	/*RefCountPointer<int> ptr = RefCountPointer<int>::MakeSharedPtr(true);

	RefCountPointer<int> ptr2 = ptr;
	*(*ptr2) = 10;

	RefCountPointer<int> ptr3 = ptr;

	printf("%d\n ", *ptr);*/

	//srand(time(NULL));

	//st_Test* tempPtr;
	//st_Test** ptr = &tempPtr;

	////*
	//for (int i = 0; i < 10000000; i++)
	//{
	//	int temp = (rand() % 1000) << 48 ;
	//	{
	//		Profiler("FindIdx");
	//		ULONGLONG idx = (temp) >> 48;
	//		*ptr = &testArr[idx];
	//	}
	//}
	////*/

	////*
	//for (int i = 0; i < 10000000; i++)
	//{
	//	int temp = rand() % 1000;
	//	{
	//		Profiler("FindIdx");
	//		for (int j = 0; j < 1000; j++)
	//		{
	//			if (temp == j)
	//			{
	//				*ptr = &testArr[j];
	//				break;
	//			}
	//		}
	//	}
	//}
	////*/

	////ProfileDataOutText("Test_Idx_2bit.txt");
	//ProfileDataOutText("Test_Idx_circuit.txt");
}