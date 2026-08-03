////#include <iostream>
////#include <Windows.h>
////
////int main()
////{
////	char* ptr = (char*)malloc(1024 * 1024 * 300);
////	DWORD max = 1024 * 1024 * 300;
////	DWORD pageSize = 1024 * 4;
////	int loop = max / pageSize;
////
////	for (int i = 0; i < loop; i++)
////	{
////		*(ptr + (i * pageSize)) = 0;
////	}
////}
//
//#include <iostream>
//#include <Windows.h>
//#include "CRingBuffer.h"
//
//DWORD Func(LPVOID arg);
//HANDLE threadArr[5];
//
//struct Type
//{
//	int a;
//	long b;
//	long long c;
//	char d;
//	short e;
//	alignas(16) short f;
//};
//
//int main()
//{
//	for(int i = 0 ;i < 5; i++)
//	{
//		threadArr[i] = CreateThread(NULL, 0, Func, NULL, 0, NULL);
//	}
//	
//	WaitForMultipleObjects(5, threadArr, TRUE, INFINITE);
//
//	printf("sizeof Ringbuffer : %d\n", sizeof(CRingBuffer));
//}
//
//// 1회 초기화
//DWORD Func(LPVOID arg)
//{
//	static DWORD bEnter = 0;
//	static DWORD bInit = 0;
//
//	Type ty;
//
//	if (bInit == 0)
//	{
//		if (InterlockedExchange(&bEnter, 1) == 0)
//		{
//			// 초기화
//			printf("초기화\n");
//			bInit = 1;
//		}
//		else
//		{
//			while(bInit == 0)
//			{ 
//				Sleep(0);
//			}
//		}
//	}
//
//	printf("Func!\n");
//
//	return 0;
//}
//
////DWORD bFlag = 0;
////// 스핀락
////DWORD Func(LPVOID arg)
////{
////	while (1)
////	{
////		while (1)
////		{
////			if (InterlockedExchange(&bFlag, 1) == 0)
////				break;
////
////			YieldProcessor();
////		}
////
////		Sleep(1000);
////
////		printf("Func! - %d\n", GetCurrentThreadId());
////
////		InterlockedExchange(&bFlag, 0);
////		//bFlag = 0;
////	}
////
////	return 0;
////}