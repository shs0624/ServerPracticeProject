#include "ProcademyProfiler.h"
#include <iostream>
#include <Windows.h>
#include <time.h>

struct st_Test
{
	ULONGLONG _sessionID;
	bool _bFlag;
};

st_Test testArr[1000];

int main()
{
	srand(time(NULL));

	st_Test* tempPtr;
	st_Test** ptr = &tempPtr;

	//*
	for (int i = 0; i < 10000000; i++)
	{
		int temp = (rand() % 1000) << 48 ;
		{
			Profiler("FindIdx");
			ULONGLONG idx = (temp) >> 48;
			*ptr = &testArr[idx];
		}
	}
	//*/

	//*
	for (int i = 0; i < 10000000; i++)
	{
		int temp = rand() % 1000;
		{
			Profiler("FindIdx");
			for (int j = 0; j < 1000; j++)
			{
				if (temp == j)
				{
					*ptr = &testArr[j];
					break;
				}
			}
		}
	}
	//*/

	//ProfileDataOutText("Test_Idx_2bit.txt");
	ProfileDataOutText("Test_Idx_circuit.txt");
}