#pragma comment(lib,"ws2_32")
#include "PacketDefine.h"
#include "Includes.h"
#include <string>
#include "ChatDummy.h"
#include "ChatDummyManager.h"
using namespace std;

procademy::CCrashDump cCrashDump;

void Input(string& serverIP, int& serverPort);

int main()
{
	ChatDummyManager* manager = new ChatDummyManager();

    string serverIP;
    int serverPort = 0;

    Input(serverIP, serverPort);

	manager->InitManager(serverIP, serverPort, 100, 100);

	while (1)
	{

	}

	return 0;
}

void Input(string& serverIP, int& serverPort)
{
    std::locale::global(std::locale("")); // 시스템이 사용하는 locale로 지정 
    cout.imbue(std::locale());

    string sserverPort;
    string disconnectTest;
    string clientCount;

    wcout << L"Server IP : ";
    getline(cin, serverIP);

    wcout << L"Server Port : ";
    getline(cin, sserverPort);

    wcout << L"ClientCount          1 = 1 / 2 = 2 / 3 = 50 / 4 = 100 / 5 = 1000 : ";
    getline(cin, clientCount);

    wcout << L"Disconnect Test      1 = YES / 2 = NO : ";
    getline(cin, disconnectTest);

    serverPort = sserverPort.empty() ? 0 : stoi(sserverPort);
    int disconnectTestValue = disconnectTest.empty() ? 0 : stoi(disconnectTest);
    int clientCountValue = clientCount.empty() ? 0 : stoi(clientCount);

     // 확인용 출력 (필요한 경우 주석 해제)
    cout << "\n입력 결과:\n";
    cout << "Server IP : " << serverIP << endl;
    cout << "Server Port : " << serverPort << endl;
    cout << "Disconnect Test : " << disconnectTestValue << endl;
    cout << "Client Count    : " << clientCountValue << endl;
}
