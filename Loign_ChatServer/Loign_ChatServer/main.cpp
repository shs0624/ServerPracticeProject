#include "Includes.h"
#include "LogManager.h"
#include "NetServer.h"
#include "LoginServer.h"

int main()
{
	LogController::GetInstance();

	LoginServer* _loginServer = new LoginServer();
	_loginServer->InitLoginServer(INADDR_ANY, SERVERPORT, true, 10000);

	char ch;
	while (1)
	{
		// ÄÁÆ®·Ñ?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			_loginServer->QuitServer();
			//break;
		}
		if (ch == 'P' || ch == 'p')
		{
			ProfileDataOutText("ProfileData.txt");
		}

	}

	return 0;
}