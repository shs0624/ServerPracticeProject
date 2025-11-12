#pragma comment(lib,"ws2_32")
#include "PacketDefine.h"
#include "Includes.h"
#include <string>
#include "ChatDummy.h"
#include "ChatDummyManager.h"
using namespace std;

procademy::CCrashDump cCrashDump;

void Input(string& serverIP, int& serverPort, int& clientCount);

int main()
{
	ChatDummyManager* manager = new ChatDummyManager();

    string serverIP;
    int serverPort = 0;
    int clientCount = 0;

    Input(serverIP, serverPort, clientCount);

	manager->InitManager(serverIP, serverPort, 100, 100);

	while (1)
	{

	}

	return 0;
}

void Input(string& serverIP, int& serverPort, int& clientCount)
{
    //std::locale::global(std::locale("")); // 시스템이 사용하는 locale로 지정 
    cout.imbue(std::locale());

    string sserverPort;
    string disconnectTest;
    string clientCountStr;

    wcout << L"Server IP : ";
    getline(cin, serverIP);

    wcout << L"Server Port : ";
    getline(cin, sserverPort);

    wcout << L"ClientCount          1 = 1 / 2 = 2 / 3 = 50 / 4 = 100 / 5 = 1000 : ";
    getline(cin, clientCountStr);

    wcout << L"Disconnect Test      1 = YES / 2 = NO : ";
    getline(cin, disconnectTest);

    serverPort = sserverPort.empty() ? 0 : stoi(sserverPort);
    int disconnectTestValue = disconnectTest.empty() ? 0 : stoi(disconnectTest);
    clientCount = clientCountStr.empty() ? 0 : stoi(clientCountStr);
    switch (clientCount)
    {
    case 1:
        clientCount = 1;
        break;
    case 2:
        clientCount = 2;
        break;
    case 3:
        clientCount = 50;
        break;
    case 4:
        clientCount = 100;
        break;
    case 5:
        clientCount = 1000;
        break;
    }

     // 확인용 출력 (필요한 경우 주석 해제)
    cout << "\n입력 결과:\n";
    cout << "Server IP : " << serverIP << endl;
    cout << "Server Port : " << serverPort << endl;
    cout << "Disconnect Test : " << disconnectTestValue << endl;
    cout << "Client Count    : " << clientCount << endl;
}
