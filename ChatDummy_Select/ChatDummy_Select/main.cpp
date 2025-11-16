#pragma comment(lib,"ws2_32")
#pragma comment(lib,"winmm.lib")
#include "Includes.h"
#include "PacketDefine.h"
#include "CommonProtocol.h"
#include <string>
#include "LogController.h"
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
    int clientCountPerThread = 0;

    Input(serverIP, serverPort, clientCountPerThread);

	manager->InitManager(serverIP, serverPort, 4, clientCountPerThread);

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

    cout << "Server IP : ";
    getline(cin, serverIP);

    cout << "Server Port : ";
    getline(cin, sserverPort);

    cout << "ClientCount Per Thread     1 = 1 / 2 = 25 / 3 = 100 / 4 = 250 / 5 = 1250 : ";
    getline(cin, clientCountStr);

    cout << "Disconnect Test            1 = YES / 2 = NO : ";
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
        clientCount = 25;
        break;
    case 3:
        clientCount = 100;
        break;
    case 4:
        clientCount = 250;
        break;
    case 5:
        clientCount = 1250;
        break;
    }

     // 확인용 출력 (필요한 경우 주석 해제)
    cout << "\n입력 결과:\n";
    cout << "Server IP : " << serverIP << endl;
    cout << "Server Port : " << serverPort << endl;
    cout << "Disconnect Test : " << disconnectTestValue << endl;
    cout << "Client Count    : " << clientCount << endl;
}
