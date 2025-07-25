#include "MessageHeader.h"

wstring str = L"Hello Monster Hunter World";
int len;

HANDLE _workerThreadArr[3];

UINT MonitorThread(LPVOID arg);
UINT WorkerThread(LPVOID arg);

int main()
{
	len = str.length();	
	g_msgQ = new CRingBuffer(50000);
	wstring test;
	st_MSG_HEAD header;

	for (int i = 0; i < 3; i++)
	{
		_workerThreadArr[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, 0, 0, NULL);
	}

	while (1)
	{
		// 키 입력 확인

		int randType = rand() % 6;
		int randlen = (rand() % len);

		header.shType = randType;
		header.shPayloadLen = randlen;
		
		//memcpy_s(test.c_str, 50, str.c_str(), randlen);

		g_msgQ->Enqueue((char*) & header, sizeof(st_MSG_HEAD));
		g_msgQ->Enqueue(test, randlen);
	}

	return 0;
}

UINT WorkerThread(LPVOID arg)
{
	printf("Thread Start! - %d\n", GetCurrentThreadId());
}

UINT MonitorThread(LPVOID arg)
{

}