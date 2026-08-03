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

void Input(string& serverIP, int& serverPort, int& clientCount, bool& isTimeoutTest, bool& bMessageFloodTest);

int main()
{
	ChatDummyManager* manager = new ChatDummyManager();

    string serverIP;
    int serverPort = 0;
    int clientCountPerThread = 0;
    bool bTimeoutTest = FALSE;
    bool bMessageFloodTest = FALSE;

    Input(serverIP, serverPort, clientCountPerThread, bTimeoutTest, bMessageFloodTest);

	manager->InitManager(serverIP, serverPort, 4, clientCountPerThread, bTimeoutTest, bMessageFloodTest);

    char ch;
	while (1)
	{
        ch = _getch();
        if ((GetAsyncKeyState('S') & 0x8001) || (GetAsyncKeyState('s') & 0x8001))
        {
            manager->OnOffManager();
        }

        Sleep(0);
	}

	return 0;
}

void Input(string& serverIP, int& serverPort, int& clientCount, bool& bTimeoutTest, bool& bMessageFloodTest)
{
    //std::locale::global(std::locale("")); // 시스템이 사용하는 locale로 지정 
    cout.imbue(std::locale());

    string sserverPort;
    string timeoutTest;
    string messageFloodTest;
    string clientCountStr;

    cout << "Server IP : ";
    getline(cin, serverIP);

    cout << "Server Port : ";
    getline(cin, sserverPort);

    cout << "ClientCount Per Thread (4 Threads)   1 = 1 / 2 = 25 / 3 = 100 / 4 = 250 / 5 = 1250 : ";
    getline(cin, clientCountStr);

    cout << "Timeout Test            1 = YES / 2 = NO : ";
    getline(cin, timeoutTest);

    cout << "Message Flood Test      1 = YES / 2 = NO : ";
    getline(cin, messageFloodTest);

    serverPort = sserverPort.empty() ? 0 : stoi(sserverPort);
    int timeoutTestValue = timeoutTest.empty() ? 0 : stoi(timeoutTest);
    int messageFloodTestValue = messageFloodTest.empty() ? 0 : stoi(messageFloodTest);
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

    if (timeoutTestValue == 1)
        bTimeoutTest = TRUE;
    else
        bTimeoutTest = FALSE;

    if (messageFloodTestValue == 1)
        bMessageFloodTest = TRUE;
    else
        bMessageFloodTest = FALSE;

    // 확인용 출력 (필요한 경우 주석 해제)
    /*cout << "\n입력 결과:\n";
    cout << "Server IP : " << serverIP << endl;
    cout << "Server Port : " << serverPort << endl;
    cout << "Timeout Test : " << timeoutTestValue << endl;
    cout << "Client Count    : " << clientCount << endl;*/
}
