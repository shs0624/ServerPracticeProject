//#pragma comment(lib,"ws2_32")
//#pragma comment(lib,"winmm.lib")
//#include <winsock2.h>
//#include <WS2tcpip.h>
//#include <process.h>
//#include <tchar.h>
//#include <time.h>
//#include <stdio.h>
//#include "IOCPClientHeader.h"
//#include "ProcademyProfiler.h"
//
//const char* str = "MonsterHunterWilds";
//int _cnt;
//
//SOCKET _socket;
//
//HANDLE _iocpHandle;
//HANDLE _logEvent;
//
//int main(void)
//{
//	_cnt = 0;
//
//	_logEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
//
//	srand(time(NULL));
//	WSADATA wsa;
//	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
//		return 1;
//
//	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
//	if (_iocpHandle == NULL) 
//		return 1;
//
//	_socket = socket(AF_INET, SOCK_STREAM, 0);
//	if (_socket == INVALID_SOCKET)
//		err_quit("socket()");
//
//	SOCKADDR_IN serverAddr;
//	ZeroMemory(&serverAddr, sizeof(serverAddr));
//	serverAddr.sin_family = AF_INET;
//	if (inet_pton(AF_INET, SERVERIP, &serverAddr.sin_addr.S_un.S_addr) != 1)
//	{
//		err_quit("inet_pton()");
//	}
//	serverAddr.sin_port = htons(SERVERPORT);
//
//	int connectRet = connect(_socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
//	if (connectRet == SOCKET_ERROR)
//		err_quit("connect");
//
//	char buf[BUFSIZE + 1];
//	int len;
//
//	while (1)
//	{
//		//클라는 그냥 보내죠?
//		int sendlen = strlen(str);
//		len = (rand() % (sendlen - 1)) + 1;
//
//		int sendret = send(_socket, str, len, 0);
//		if (sendret == SOCKET_ERROR)
//		{
//			err_display("send()");
//			break;
//		}
//
//		// 리시브가 필요한가?
//		_cnt++;
//	}
//}
//
//unsigned int WINAPI LogThread(LPVOID arg)
//{
//	while (1)
//	{
//		WaitForSingleObject(_logEvent, 1000);
//
//		printf("[LOG] Cnt : %d\n", _cnt);
//	}
//}
//
//// 소켓 함수 오류 출력 후 종료
//inline void err_quit(const char* msg)
//{
//	int err = WSAGetLastError();
//	printf("[%s] TCP Error Number : %d\n", msg, err);
//	exit(1);
//}
//
//inline void err_display(const char* msg)
//{
//	int err = WSAGetLastError();
//	printf("[%s] TCP Error Number : %d\n", msg, err);
//	return;
//}