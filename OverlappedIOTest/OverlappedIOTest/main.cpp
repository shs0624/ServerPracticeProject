#include "IOCPTestHeader.h"
using namespace std;

SOCKET _socket;

int interval;
int len;
string str = "Hello Monster Hunter World";

ULONGLONG totalTime;
int Count;

void err_quit(const char* msg)
{
	int err = WSAGetLastError();
	printf("[%d] last Error NUM\n", err);
	exit(1);
}

void err_display(const char* msg)
{
	int err = WSAGetLastError();
	printf("[%d] last Error NUM\n", err);
}

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0)
	{
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;

		left -= received;
		ptr += received;
	}

	return (len - left);
}

bool InitConnect()
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	// socket()
	_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (_socket == INVALID_SOCKET)
		err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;

	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	if (inet_pton(AF_INET, SERVERIP, &serveraddr.sin_addr.S_un.S_addr) != 1)
	{
		err_quit("inet_pton()");
	}
	serveraddr.sin_port = htons(SERVERPORT);
	retval = connect(_socket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("connect()");

	return true;
}

void SetSendInterval()
{
	int interval = 0;

	printf("Enter Send Interval : ");
	scanf_s("%d", &interval);
}

// 서버에 랜덤 문자열 길이를 계속 보내는 클라이언트.
// 처음에 보내는 주기를 설정하게 해서 보내는 양을 조절하게 하자.
int main(int argc, char* argv[])
{
	int retval;
	srand(time(NULL));
	
	if (!InitConnect())
		err_quit("Init Fail");

	// 필요하면 인터벌 설정
	//SetSendInterval();

	len = str.length();

	// 데이터 통신에 사용할 변수
	char buf[BUFSIZE + 1];
	char recvbuf[BUFSIZE + 1];
	char sendTempPage[5000];
	char recvTempPage[5000];
	string temp;
	LARGE_INTEGER sendTime;
	LARGE_INTEGER recvTime;
	ULONGLONG roundTime;

	// 서버와 데이터 통신
	while (1)
	{
		if (GetAsyncKeyState(VK_SPACE))
		{
			break;
		}

		// 데이터 입력
		size_t randlen = 1 + (rand() % (len - 1));
		temp = str.substr(0, randlen);

		// 데이터 보내기
		QueryPerformanceCounter(&sendTime);
		//retval = send(_socket, sendTempPage, sizeof(sendTempPage), 0);
		retval = send(_socket, temp.c_str(), temp.size(), 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("send()");
			break;
		}
		//printf("[TCP 클라이언트] %d바이트를 보냈습니다.\n", retval);

		// 데이터 받기
		//retval = recvn(_socket, recvbuf, retval, 0);
		//retval = recv(_socket, recvTempPage, sizeof(recvTempPage), 0);
		retval = recv(_socket, recvbuf, sizeof(recvbuf), 0);
		if (retval == SOCKET_ERROR)
		{
			err_display("recv()");
			break;
		}
		else if (retval == 0)
			break;

		QueryPerformanceCounter(&recvTime);
		roundTime = recvTime.QuadPart - sendTime.QuadPart;

		totalTime += roundTime;
		Count++;
		// 받은 데이터 출력
		//recvbuf[retval] = '\0';
		//printf("[TCP 클라이언트] %d바이트를 받았습니다.\n", retval);
		//printf("[TCP 클라이언트] 소요 시간 : %lld.\n", roundTime);
		//printf("[받은 데이터] %s\n", recvbuf);
	}

	printf("---------------------------------------------------------------\n");
	printf("Average RTT : %lld\n", totalTime / Count);
	printf("---------------------------------------------------------------\n");

	closesocket(_socket);

	WSACleanup();
	return 0;
}