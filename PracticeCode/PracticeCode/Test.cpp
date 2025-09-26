#include <iostream>
#include <Windows.h>

DWORD Func(LPVOID arg);
HANDLE threadArr[5];

int main()
{
	for(int i = 0 ;i < 5; i++)
	{
		threadArr[i] = CreateThread(NULL, 0, Func, NULL, 0, NULL);
	}
	
	WaitForMultipleObjects(5, threadArr, TRUE, INFINITE);
}

DWORD Func(LPVOID arg)
{
	static DWORD bEnter = 0;
	static DWORD bInit = 0;
	if (bInit == 0)
	{
		if (InterlockedExchange(&bEnter, 1) == 0)
		{
			// 초기화
			printf("초기화\n");
			bInit = 1;
		}
		else
		{
			while(bInit == 0)
			{ }
		}
	}

	printf("Func!\n");

	return 0;
}