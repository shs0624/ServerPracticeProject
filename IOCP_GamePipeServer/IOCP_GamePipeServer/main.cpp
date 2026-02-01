#include "Includes.h"
#include "LogManager.h"
#include "NetServer_Pipe.h"
#include "IRoom.h"
#include "AuthRoom.h"
#include "EchoRoom.h"
#include "IRoomFactory.h"
#include "RoomNetServer.h"

procademy::CCrashDump cCrashDump;

int main()
{
	//LogController::GetInstance();
	timeBeginPeriod(1);

	RoomNetServer* _gameServer = new RoomNetServer();
	_gameServer->InitRoomNetServer(INADDR_ANY, SERVERPORT, true, 10000);

	char ch;
	while (1)
	{
		// ÄÁÆ®·Ñ?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			_gameServer->QuitServer();
			//break;
		}
		if (ch == 'P' || ch == 'p')
		{
			ProfileDataOutText("ProfileData.txt");
		}

	}

	timeEndPeriod(1);

	return 0;
}