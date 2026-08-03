#include "EventQueueHeader.h"

wstring str = L"PlayEternalReturnFree";
const char* listFileName = "EventQueueSaveText.txt";

HANDLE WorkerThreadHandle[3];
HANDLE WorkerThreadEvent;

DWORD WorkerThreadID[3];

SRWLOCK _srwLock;

int wmain(void)
{
	g_msgQ = new CRingBuffer(50000);
	WorkerThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	InitializeSRWLock(&_srwLock);

	for (int i = 0; i < 3; i++)
	{
		WorkerThreadHandle[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)WorkerThreadID[i]);
	}

	while (1)
	{

	}
}

unsigned int WorkerThread(LPVOID lpParam)
{
	while (1)
	{
		WaitForSingleObject(WorkerThreadEvent, INFINITE);


	}
}

UINT AddListFunc(LPVOID lpParameter)
{
	srand(time(NULL));
	DWORD dwResult;
	while (1)
	{
		int input = rand() % 999;

		AcquireSRWLockExclusive(&_srwLock);

		ReleaseSRWLockExclusive(&_srwLock);
	}

	return 0;
}

UINT DeleteListFunc(LPVOID lpParameter)
{
	while (1)
	{
		AcquireSRWLockExclusive(&_srwLock);

		ReleaseSRWLockExclusive(&_srwLock);
	}

	return 0;
}

UINT PrintListFunc(LPVOID lpParameter)
{
	while (1)
	{
		AcquireSRWLockExclusive(&_srwLock);

		ReleaseSRWLockExclusive(&_srwLock);
	}

	return 0;
}

UINT SaveListFunc(LPVOID lpParameter)
{
	string saveStr;
	while (1)
	{
		AcquireSRWLockExclusive(&_srwLock);

		ReleaseSRWLockExclusive(&_srwLock);

		if (!saveStr.empty())
		{
			FILE* fptr;
			fopen_s(&fptr, listFileName, "wb");
			if (fptr == nullptr)
			{
				throw 0;
			}

			fwrite(saveStr.c_str(), saveStr.size(), 1, fptr);
			fclose(fptr);
			saveStr.clear();
		}
	}

	return 0;
}