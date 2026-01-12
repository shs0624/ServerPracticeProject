#include "Includes.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "LogManager.h"

procademy::CCrashDump cCrashDump;

int main()
{
	//LogController::GetInstance();

	ChatServer* _chatServer = new ChatServer();
	_chatServer->InitChatServer(INADDR_ANY, SERVERPORT, true, 16000);

	char ch;
	while (1)
	{
		// ÄÁÆ®·Ñ?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			_chatServer->QuitServer();
			//break;
		}
		if (ch == 'P' || ch == 'p')
		{
			ProfileDataOutText("ProfileData.txt");
		}

	}

	return 0;
}