#include <iostream>
#include <Windows.h>
#include <process.h>
#include <list>
#include <string>
using namespace std;

const char* listFileName = "ListSaveText.txt";

HANDLE addThreadEvent[3];
HANDLE deleteThreadEvent;
HANDLE printThreadEvent;
HANDLE saveThreadEvent;

list<int> _numList;

SRWLOCK _srwLock;

bool bFlag;

void Input()
{
	if (GetAsyncKeyState(VK_ESCAPE))
	{
		// 종료
		bFlag = true;

		SetEvent(saveThreadEvent);
	}

	if (GetAsyncKeyState(VK_SPACE))
	{
		// 저장
		SetEvent(saveThreadEvent);
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
		_numList.push_back(input);
		ReleaseSRWLockExclusive(&_srwLock);

		dwResult = WaitForSingleObject(addThreadEvent[(int)lpParameter], 1000);
		
		if (bFlag)
			break;
	}

	return 0;
}

UINT DeleteListFunc(LPVOID lpParameter)
{
	while (1)
	{
		AcquireSRWLockExclusive(&_srwLock);
		if (!_numList.empty())
		{
			_numList.pop_front();
		}
		ReleaseSRWLockExclusive(&_srwLock);

		WaitForSingleObject(deleteThreadEvent, 333);

		if (bFlag)
			break;
	}

	return 0;
}

UINT PrintListFunc(LPVOID lpParameter)
{
	while (1)
	{
		AcquireSRWLockExclusive(&_srwLock);
		if (!_numList.empty())
		{
			list<int>::iterator it = _numList.begin();
			printf("%d", *it);
			it++;
			for (; it != _numList.end(); it++)
				printf("-%d", *it);

			printf("\n");
		}
		ReleaseSRWLockExclusive(&_srwLock);
		
		WaitForSingleObject(printThreadEvent, 1000);

		if (bFlag)
			break;
	}

	return 0;
}

UINT SaveListFunc(LPVOID lpParameter)
{
	string saveStr;
	while (1)
	{
		AcquireSRWLockExclusive(&_srwLock);
		if (!_numList.empty())
		{
			list<int>::iterator it = _numList.begin();
			saveStr.append(to_string(*it));

			for (; it != _numList.end(); it++)
			{
				saveStr.append("-" + to_string(*it));
			}
		}
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

		WaitForSingleObject(saveThreadEvent, INFINITE);

		if (bFlag)
			break;
	}

	return 0;
}

int main()
{
	HANDLE workerThread[6];
	HANDLE addThreadArr[3];
	HANDLE deleteThread;
	HANDLE printThread;
	HANDLE saveThread;

	HANDLE addEvent;
	BOOL bStatus;

	bFlag = false;
	int cnt = 0;
	
	InitializeSRWLock(&_srwLock);

	for (int i = 0; i < 3; i++)
	{
		addThreadEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		addThreadArr[i] = (HANDLE)_beginthreadex(NULL, 0, AddListFunc, (void*)i, 0, NULL);
		workerThread[cnt++] = addThreadArr[i];
	}

	deleteThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	deleteThread = (HANDLE)_beginthreadex(NULL, 0, DeleteListFunc, NULL, 0, NULL);
	workerThread[cnt++] = deleteThread;

	printThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	printThread = (HANDLE)_beginthreadex(NULL, 0, PrintListFunc, NULL, 0, NULL);
	workerThread[cnt++] = printThread;

	saveThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	saveThread = (HANDLE)_beginthreadex(NULL, 0, SaveListFunc, NULL, 0, NULL);
	workerThread[cnt++] = saveThread;


	while (1)
	{
		Input();

		if (bFlag)
			break;
	}

	WaitForMultipleObjects(cnt, workerThread, TRUE, INFINITE);

	for (int i = 0; i < 3; i++)
	{
		CloseHandle(addThreadArr[i]);
	}
	CloseHandle(printThread);
	CloseHandle(deleteThread);
	CloseHandle(saveThread);

	return 0;
}