#include "MessageHeader.h"
#include <wchar.h>

//-----------------------------------------------
// 컨텐츠 부, 문자열 리스트
//-----------------------------------------------
list<wstring> g_List;

//-----------------------------------------------
// 스레드 메시지 큐 (사이즈 넉넉하게 크게 4~5만 바이트)
//-----------------------------------------------
CRingBuffer* g_msgQ;

SRWLOCK _srwLock;

wstring str = L"Hello Monster Hunter World";
int len;

HANDLE _workerThreadArr[3];
HANDLE _workerThreadEvent;
HANDLE _monitorThread;

DWORD _threadTPS[3];
DWORD _typeTPS[5];
DWORD _totalTPS;

UINT MonitorThread(LPVOID arg);
UINT WorkerThread(LPVOID arg);

int main()
{
	srand(time(NULL));
	len = str.length();	
	g_msgQ = new CRingBuffer(50000);
	wstring test;
	st_MSG_HEAD header;

	InitializeSRWLock(&_srwLock);

	_workerThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	for (int i = 0; i < 3; i++)
	{
		_workerThreadArr[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)i, 0, NULL);
	}
	_monitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, 0, 0, NULL);

	while (1)
	{
		// 키 입력 확인
		int randType = rand() % 5;
		size_t randlen = (rand() % len);

		if (GetAsyncKeyState(VK_ESCAPE))
		{
			// 종료
			header.shType = dfJOB_QUIT;
			header.shPayloadLen = 0;

			for(int i = 0; i < 3; i++)
				g_msgQ->Enqueue((char*)&header, sizeof(st_MSG_HEAD));

			break;
		}

		header.shType = randType;
		header.shPayloadLen = randlen * sizeof(wchar_t);
		
		test = str.substr(0, randlen);
		//memcpy_s((void*)test.c_str(), len * sizeof(wchar_t), (void*)str.c_str(), randlen * sizeof(wchar_t));

		//AcquireSRWLockExclusive(&_srwLock);
		g_msgQ->Enqueue((char*) & header, sizeof(st_MSG_HEAD));
		g_msgQ->Enqueue((char*)test.c_str(), randlen * sizeof(wchar_t));
		//ReleaseSRWLockExclusive(&_srwLock);

		SetEvent(_workerThreadEvent);

		Sleep(10);
	}

	WaitForMultipleObjects(3, _workerThreadArr, TRUE, INFINITE);

	return 0;
}

UINT WorkerThread(LPVOID arg)
{
	printf("Thread Start! - %d\n", GetCurrentThreadId());
	int threadNum = (int)arg;
	//wstring temp;
	list<wstring>::iterator it;
	st_MSG_HEAD header;
	wchar_t temp[100];
	while (1)
	{
		WaitForSingleObject(_workerThreadEvent, INFINITE);
 		ZeroMemory(temp, sizeof(temp));

		AcquireSRWLockExclusive(&_srwLock);
		if (g_msgQ->GetUseSize() < sizeof(st_MSG_HEAD))
		{
			ReleaseSRWLockExclusive(&_srwLock);
			continue;
		}

		g_msgQ->Peek((char*)&header, sizeof(st_MSG_HEAD));

		if (g_msgQ->GetUseSize() < sizeof(st_MSG_HEAD) + header.shPayloadLen)
		{
			ReleaseSRWLockExclusive(&_srwLock);
			continue;
		}

		g_msgQ->MoveFront(sizeof(st_MSG_HEAD));
		g_msgQ->Dequeue((char*)temp, header.shPayloadLen);
		ReleaseSRWLockExclusive(&_srwLock);

		switch (header.shType)
		{
		case dfJOB_ADD:
			g_List.push_back(temp);
			break;
		case dfJOB_DEL:
			if(g_List.size() != 0)
				g_List.pop_back();
			break;
		case dfJOB_SORT:
			g_List.sort();
			break;
		case dfJOB_FIND:
			for (it = g_List.begin(); it != g_List.end(); it++)
			{
				if ((*it).compare(temp) == 0)
					break;
			}
			break;
		case dfJOB_PRINT:
			for (it = g_List.begin(); it != g_List.end(); it++)
			{
				wcout << *it << endl;
				//wprintf(L"%s\n", *it);
			}
			break;
		case dfJOB_QUIT:
			return 0;
		}

		InterlockedIncrement(&_totalTPS);
		InterlockedIncrement(&_threadTPS[threadNum]);
		InterlockedIncrement(&_typeTPS[header.shType]);
	}
}

UINT MonitorThread(LPVOID arg)
{
	HANDLE _hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	while (1)
	{
		WaitForSingleObject(_hEvent, 1000);

		printf("-----------------------------------\n");
		printf("Msg Queue Use Size : %d\n", g_msgQ->GetUseSize());
		printf("Total TPS : %d\n", _totalTPS);
		printf("thread 1TPS : %d\nthread 2TPS : %d\nthread 3TPS : %d\n",
			_threadTPS[0], _threadTPS[1], _threadTPS[2]);
		printf("ADD TPS : %d\nDEL TPS : %d\nSORT TPS : %d\nFIND TPS : %d\nPRINT TPS : %d\nQUIT TPS : %d\n",
			_typeTPS[0], _typeTPS[1], _typeTPS[2], _typeTPS[3], _typeTPS[4], _typeTPS[5]);
		printf("-----------------------------------\n");

		_totalTPS = 0;
		ZeroMemory(_threadTPS, sizeof(_threadTPS));
		ZeroMemory(_typeTPS, sizeof(_typeTPS));
	}
}