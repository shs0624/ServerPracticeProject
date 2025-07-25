//#pragma comment(lib,"winmm.lib")
//#include <iostream>
//#include <Windows.h>
//#include <process.h>
//
//HANDLE UpdatethreadHandleArr[3];
//HANDLE AcceptThreadHandle;
//HANDLE DisconnectThreadHandle;
//
//HANDLE threadHandleList[5];
//
//HANDLE MainEventHandle;
//HANDLE UpdateEventHandle;
//HANDLE AcceptEventHandle;
//HANDLE DisconnectEventHandle;
//
//DWORD g_Data = 0;
//DWORD g_Connect = 0;
//bool g_Shutdown = false;
//
//UINT AcceptThread(LPVOID arg);
//UINT DisconnectThread(LPVOID arg);
//UINT UpdateThread(LPVOID arg);
//
//int main()
//{
//    timeBeginPeriod(1);
//    int cnt = 0;
//
//    for (int i = 0; i < 3; i++)
//    {
//        UpdatethreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, (LPVOID)(i + 1), 0, NULL);
//    }
//    AcceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)0, 0, NULL);
//    DisconnectThreadHandle = (HANDLE)_beginthreadex(NULL, 0, DisconnectThread, (LPVOID)0, 0, NULL);
//
//    for (int i = 0; i < 3; i++)
//    {
//        threadHandleList[i] = UpdatethreadHandleArr[i];
//    }
//    threadHandleList[3] = AcceptThreadHandle;
//    threadHandleList[4] = DisconnectThreadHandle;
//
//    UpdateEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
//    AcceptEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
//    DisconnectEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
//    MainEventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
//
//    while (1)
//    {
//        WaitForSingleObject(MainEventHandle, 1000);
//
//        printf("        g_Connect : %d\n", g_Connect);
//        cnt++;
//
//        if (cnt == 20)
//        {
//            g_Shutdown = true;
//            break;
//        }
//    }
//    
//    WaitForMultipleObjects(5, threadHandleList, TRUE, INFINITE);
//
//    return 0;
//}
//
//UINT AcceptThread(LPVOID arg)
//{
//    while (1)
//    {
//        if (g_Shutdown)
//            break;
//
//        DWORD randTime = 100 + (rand() % 900);
//        WaitForSingleObject(UpdateEventHandle, randTime);
//
//        InterlockedIncrement(&g_Connect);
//    }
//
//    return 0;
//}
//
//UINT DisconnectThread(LPVOID arg)
//{
//    while (1)
//    {
//        if (g_Shutdown)
//            break;
//
//        DWORD randTime = 100 + (rand() % 900);
//        WaitForSingleObject(UpdateEventHandle, randTime);
//
//        if (g_Connect > 0)
//        {
//            InterlockedDecrement(&g_Connect);
//        }
//    }
//
//    return 0;
//}
//
//UINT UpdateThread(LPVOID arg)
//{
//    while (1)
//    {
//        if (g_Shutdown)
//            break;
//
//        WaitForSingleObject(UpdateEventHandle, 10);
//
//        int result = InterlockedIncrement(&g_Data);
//        if (result % 1000 == 0)
//        {
//            printf("%d | g_Data : %d\n", (int)arg, result);
//        }
//    }
//
//    return 0;
//}