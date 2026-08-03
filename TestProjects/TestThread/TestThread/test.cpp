#include <Windows.h>
#include <iostream>

void WorkerThreadProc();

int main()
{
	HANDLE hWorkerThread;
	DWORD dwWorkerThreadId;
	DWORD dwPrimaryThreadId;

	dwPrimaryThreadId = GetCurrentThreadId();

	printf("[%08x] Multithreaded Hello application started.\n", dwPrimaryThreadId);

	hWorkerThread = CreateThread(NULL, 0, WorkerThreadProc, (LPVOID)3, 0, &dwWorkerThreadId);

	if (hWorkerThread != NULL)
	{
		printf("[%08x] Worker thread ID = 0x%08x.\n", dwPrimaryThreadId, dwWorkerThreadId);

		Sleep(6000);

		DWORD dwThreadExitCode;

		GetExitCodeThread(hWorkerThread, &dwThreadExitCode);

		printf("[%08x] Worker thread exit code = 0x%08x.\n", dwPrimaryThreadId, dwThreadExitCode);
	}
	else
	{
		//printf()
	}

	return 0;
}