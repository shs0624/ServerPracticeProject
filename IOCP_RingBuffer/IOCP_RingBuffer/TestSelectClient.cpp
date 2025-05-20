//#pragma comment(lib,"ws2_32")
//#pragma comment(lib,"winmm.lib")
//#include <winsock2.h>
//#include <WS2tcpip.h>
//#include <iostream>
//#include <conio.h>
//#include <Windows.h>
//#include "TestSelectClient.h"
//
//const char* str = "MonsterHunterWilds";
//int _sendCnt;
//int _recvCnt;
//
//SOCKET _socket;
//
//HANDLE _logEvent;
//
//int main(void)
//{
//	_sendCnt = 0;
//	_recvCnt = 0;
//
//	_logEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
//
//	srand(time(NULL));
//	WSADATA wsa;
//	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
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
//		fd_set readSet;
//		FD_ZERO(&readSet);
//		FD_SET(_socket, &readSet);
//
//		timeval time;
//		time.tv_sec = 0;
//		time.tv_usec = 0;
//
//		int selectRet = select(0, &readSet, 0, 0, &time);
//		if (selectRet == SOCKET_ERROR)
//			err_quit("select()");
//
//		char ch = _getch();
//		if (ch == 'E' || ch == 'e')
//		{
//			int sendlen = strlen(str);
//			len = (rand() % (sendlen - 1)) + 1;
//
//			int sendret = send(_socket, str, len, 0);
//			if (sendret == SOCKET_ERROR)
//			{
//				err_display("send()");
//				break;
//			}
//
//			_sendCnt++;
//		}
//
//		if (FD_ISSET(_socket, &readSet))
//		{
//			int recvRet = recv(_socket, buf, BUFSIZE, 0);
//			if (recvRet == SOCKET_ERROR || recvRet == 0)
//			{
//				if (recvRet == WSAEWOULDBLOCK)
//					return 0;
//
//				err_quit("recv()");
//				return 0;
//			}
//
//			_recvCnt++;
//		}
//	}
//
//	return 0;
//}
//
//unsigned int WINAPI LogThread(LPVOID arg)
//{
//	while (1)
//	{
//		WaitForSingleObject(_logEvent, 1000);
//
//		printf("[LOG] SendCnt : %d | RecvCnt : %d\n", _sendCnt, _recvCnt);
//
//		_sendCnt = 0;
//		_recvCnt = 0;
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