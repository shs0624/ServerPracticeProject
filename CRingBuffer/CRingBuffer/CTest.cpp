#include "CRingBuffer.h"
#include <iostream>
#include <Windows.h>
#include <process.h>

char str[50] = "Hello Monster Hunter World";

struct st_HEADER
{
	int iCode; // 0x89
	int iSize; // 헤더 제외
};

UINT EnqueueThread(LPVOID lpThreadParameter);
UINT DequeueThread(LPVOID lpThreadParameter);
void EnqueueDequeue();

CRingBuffer* pRingBuffer;

int main()
{
	HANDLE threadArr[2];

	HANDLE hThread1;
	HANDLE hThread2;

	UINT dwThread1Id;
	UINT dwThread2Id;

	int size = strlen(str);

	pRingBuffer = new CRingBuffer(500);

	/*while (1)
	{
		EnqueueDequeue();
	}*/

	threadArr[0] = (HANDLE)_beginthreadex(NULL, 0, EnqueueThread, (LPVOID)size, 0, &dwThread1Id);
	threadArr[1] = (HANDLE)_beginthreadex(NULL, 0, DequeueThread, (LPVOID)size, 0, &dwThread2Id);

	WaitForMultipleObjects(2, threadArr, true, INFINITE);

	return 0;
}

void EnqueueDequeue()
{
	srand(time(NULL));
	char buffer[50];
	int len = strlen(str);
	while (1)
	{
		int EnqueueCnt = (rand() % 30) + 1;
		for (int i = 0; i < EnqueueCnt; i++)
		{
			int strSize = (rand() % (len - 1)) + 1;

			st_HEADER header;
			header.iCode = 0x89;
			header.iSize = strSize;
			if (pRingBuffer->GetFreeSize() < sizeof(st_HEADER) + header.iSize)
			{
				break;
			}

			memcpy_s(buffer, 50, &header, sizeof(st_HEADER));
			memcpy_s(buffer + sizeof(st_HEADER), strSize, &str, strSize);
			buffer[sizeof(st_HEADER) + strSize] = '\0';

			int ret = pRingBuffer->Enqueue(buffer, sizeof(st_HEADER) + header.iSize);
			if (ret != sizeof(st_HEADER) + header.iSize)
				DebugBreak();

			printf("Enqeue Result : %s | UseSize : %d\n", buffer + sizeof(st_HEADER), pRingBuffer->GetUseSize());
		}

		int DequeueCnt = (rand() % 10) + 1;
		for (int i = 0; i < DequeueCnt; i++)
		{
			st_HEADER header;
			if (pRingBuffer->GetUseSize() < sizeof(st_HEADER))
			{
				break;
			}

			int peekRet = pRingBuffer->Peek((char*)&header, sizeof(st_HEADER));
			if (peekRet != sizeof(st_HEADER))
				DebugBreak();

			if (header.iCode != 0x89)
				DebugBreak();

			if (pRingBuffer->GetUseSize() < sizeof(st_HEADER) + header.iSize)
				DebugBreak();

			int dequeueRet = pRingBuffer->Dequeue(buffer, peekRet + header.iSize);
			if (dequeueRet != peekRet + header.iSize)
				DebugBreak();

			buffer[sizeof(st_HEADER) + header.iSize] = '\0';
			printf("Dequeue Result : %s | UseSize : %d\n", buffer + sizeof(st_HEADER), pRingBuffer->GetUseSize());
		}
	}
}

UINT EnqueueThread(LPVOID lpThreadParameter)
{
	srand(time(NULL));
	int size = (int)lpThreadParameter;

	char buffer[50];
	while (1)
	{
		int strSize = (rand() % (size - 1)) + 1; 

		st_HEADER header;
		header.iCode = 0x89;
		header.iSize = strSize;
		if (pRingBuffer->GetFreeSize() < sizeof(st_HEADER) + header.iSize)
		{
			continue;
		}

		memcpy_s(buffer, 50, &header, sizeof(st_HEADER));
		memcpy_s(buffer + sizeof(st_HEADER), strSize, &str, strSize);
		buffer[sizeof(st_HEADER) + strSize] = '\0';

		int ret = pRingBuffer->Enqueue(buffer, sizeof(st_HEADER) + header.iSize);
		if (ret != sizeof(st_HEADER) + header.iSize)
			DebugBreak();

		printf("Enqeue Result : %s | UseSize : %d\n", buffer + sizeof(st_HEADER), pRingBuffer->GetUseSize());
	}
}

UINT DequeueThread(LPVOID lpThreadParameter)
{
	srand(time(NULL));
	int size = (int)lpThreadParameter;

	char buffer[100];
	while (1)
	{
		st_HEADER header;
		if (pRingBuffer->GetUseSize() < sizeof(st_HEADER))
		{
			continue;
		}

		int peekRet = pRingBuffer->Peek((char*)&header, sizeof(st_HEADER));
		if (peekRet != sizeof(st_HEADER))
			DebugBreak();

		if (header.iCode != 0x89)
			DebugBreak();

		if (pRingBuffer->GetUseSize() < sizeof(st_HEADER) + header.iSize)
			DebugBreak();

		int dequeueRet = pRingBuffer->Dequeue(buffer, peekRet + header.iSize);
		if (dequeueRet != peekRet + header.iSize)
			DebugBreak();

		buffer[sizeof(st_HEADER) + header.iSize] = '\0';
		printf("Dequeue Result : %s | UseSize : %d\n", buffer + sizeof(st_HEADER), pRingBuffer->GetUseSize());
	}
}