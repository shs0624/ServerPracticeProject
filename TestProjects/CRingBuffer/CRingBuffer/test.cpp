//#include <iostream>
//#include <Windows.h>
//#include <thread>
//#include "CRingBuffer.h"
//
//using namespace std;
//
//const char* str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
//char Buf[200];
//CRingBuffer serverRingBuffer(500);
//CRingBuffer clientRingBuffer(500);
//
//int serverIdx = 0;
//int clientIdx = 0;
//
//CRITICAL_SECTION cs;
//
//void PrintRingBuffer()
//{
//	int front = serverRingBuffer.head;
//	int rear = serverRingBuffer.tail;
//
//	printf("RingBuffer: ");
//	while (1)
//	{
//		if (front == rear) break;
//
//		printf("%c", serverRingBuffer.arr[front]);
//		front = (front + 1) % serverRingBuffer.max;
//	}
//
//	printf("\n");
//}
//
//void EnqueueThread()
//{
//	srand(time(NULL));
//
//	while (1)
//	{
//		//cout << "Enqueue 시작" << endl;
//		int cntNum = (rand() % 3) + 1;
//
//		for (int i = 0; i < cntNum; i++)
//		{
//			int cutOffNum = rand() % 20;
//
//			if (serverIdx + cutOffNum > strlen(str))
//				cutOffNum = strlen(str) - serverIdx;
//
//			//EnterCriticalSection(&cs);
//			int ret = serverRingBuffer.Enqueue(str + serverIdx, cutOffNum);
//			//LeaveCriticalSection(&cs);
//
//			serverIdx = (serverIdx + ret) % strlen(str);
//
//			//PrintRingBuffer();
//
//		}
//		//cout << "Enqueue 끝" << endl << endl;
//	}
//}
//
//void DequeueThread()
//{
//	srand(time(NULL));
//
//	int cnt = 0;
//
//	while (1)
//	{
//		//cout << "Dequeue 시작" << endl;
//		int cntNum2 = (rand() % 3) + 1;
//
//		for (int i = 0; i < cntNum2; i++)
//		{
//			int cutOffNum2 = rand() % 20;
//
//			//EnterCriticalSection(&cs);
//			int ret = serverRingBuffer.Dequeue(Buf, cutOffNum2);
//			//LeaveCriticalSection(&cs);
//
//			//PrintRingBuffer();
//			//cout << "Buf : "<< Buf << endl;
//
//
//			//cout << Buf;
//
//			char tmp1[50];
//			char tmp2[50];
//			if (clientIdx + ret > strlen(str))
//			{
//				memset(tmp1, 0, 50);
//				memset(tmp2, 0, 50);
//
//				int oneStep = strlen(str) - clientIdx;
//				int twoStep = ret - oneStep;
//
//				memcpy_s(tmp1, 50, str + clientIdx, oneStep);
//				memcpy_s(tmp2, 50, str, twoStep);
//				strcat_s(tmp1, tmp2);
//			}
//			else
//			{
//				memcpy_s(tmp1, 50, str + clientIdx, ret);
//			}
//
//			if (strncmp(Buf, tmp1, ret) != 0)
//			{
//				DebugBreak();
//			}
//
//			clientIdx = (clientIdx + ret) % strlen(str);
//
//			memset(Buf, 0, 200);
//		}
//
//		//cout << "Dequeue 끝" << endl << endl;
//	}
//}
//
//void Single()
//{
//	srand(time(NULL));
//
//
//	while (1)
//	{
//		//cout << "Enqueue 시작" << endl;
//		int cntNum = (rand() % 3) + 1;
//
//		for (int i = 0; i < cntNum; i++)
//		{
//			int cutOffNum = rand() % 20;
//
//			if (serverIdx + cutOffNum > strlen(str))
//				cutOffNum = strlen(str) - serverIdx;
//
//			int ret = serverRingBuffer.Enqueue(str + serverIdx, cutOffNum);
//
//			serverIdx = (serverIdx + ret) % strlen(str);
//
//			//PrintRingBuffer();
//
//		}
//		//cout << "Enqueue 끝" << endl << endl;
//
//		//cout << "Dequeue 시작" << endl;
//		int cntNum2 = (rand() % 3) + 1;
//
//		for (int i = 0; i < cntNum2; i++)
//		{
//			int cutOffNum2 = rand() % 20;
//
//			int ret = serverRingBuffer.Dequeue(Buf, cutOffNum2);
//
//			//PrintRingBuffer();
//			//cout << "Buf : "<< Buf << endl;
//
//
//			cout << Buf;
//
//			char tmp1[50];
//			char tmp2[50];
//			if (clientIdx + ret > strlen(str))
//			{
//				memset(tmp1, 0, 50);
//				memset(tmp2, 0, 50);
//
//				int oneStep = strlen(str) - clientIdx;
//				int twoStep = ret - oneStep;
//
//				memcpy_s(tmp1, 50, str + clientIdx, oneStep);
//				memcpy_s(tmp2, 50, str, twoStep);
//				strcat_s(tmp1, tmp2);
//			}
//			else
//			{
//				memcpy_s(tmp1, 50, str + clientIdx, ret);
//			}
//
//			if (strncmp(Buf, tmp1, ret) != 0)
//			{
//				DebugBreak();
//			}
//
//			clientIdx = (clientIdx + ret) % strlen(str);
//
//			memset(Buf, 0, 200);
//		}
//
//		//cout << "Dequeue 끝" << endl << endl;
//	}
//}
//
//int main()
//{
//	InitializeCriticalSection(&cs);
//
//	HANDLE threadHandles[2];
//
//	threadHandles[0] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)&EnqueueThread, NULL, NULL, NULL);
//	threadHandles[1] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)&DequeueThread, NULL, NULL, NULL);
//
//	WaitForMultipleObjects(2, threadHandles, true, INFINITE);
//
//	/*
//	HANDLE threadHandle;
//	threadHandle = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)&Single, NULL, NULL, NULL);
//
//	WaitForSingleObject(threadHandle, INFINITE);
//	*/
//
//	return 0;
//}