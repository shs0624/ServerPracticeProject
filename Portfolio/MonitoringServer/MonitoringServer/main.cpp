#include "Includes.h"
#include "LanServer.h"
#include "NetServer.h"
#include "LogManager.h"
#include "MonitorProtocol.h"
#include "MonitorClientServer.h"
#include "MonitorDataManager.h"
#include "MonitorChatServer.h"

#define SERVER_FIXED_KEY 0x32
#define SERVER_PROGRAM_KEY 0x77

#define CLIENT_FIXED_KEY 30
#define CLIENT_PROGRAM_KEY 109

int main()
{
	LogController::GetInstance();

	MonitorDataManager* pManager = new MonitorDataManager();

	MonitorChatServer* pChatServer = new MonitorChatServer();
	pChatServer->InitMonitorChatServer(pManager, INADDR_ANY, 100, SERVER_PROGRAM_KEY, SERVER_FIXED_KEY);

	MonitorClientServer* pClientServer = new MonitorClientServer();
	pClientServer->InitMonitorClientServer(INADDR_ANY, 100, CLIENT_PROGRAM_KEY, CLIENT_FIXED_KEY);

	pManager->InitDataManager(pClientServer);
	while (1)
	{
		Sleep(0);
	}

	return 0;
}