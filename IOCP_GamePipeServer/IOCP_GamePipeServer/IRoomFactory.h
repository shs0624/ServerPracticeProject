#pragma once
class IRoomFactory
{
public:
	static IRoom* Create(DWORD roomNumber)
	{
		IRoom* ptr = NULL;
		// RoomNumber에 따라 다른 클래스 할당
		switch (roomNumber)
		{
		case dfROOM_AUTH:
			ptr = (IRoom*)new AuthRoom();
			break;
		case dfROOM_ECHO:
			ptr = (IRoom*)new EchoRoom();
			break;
		}

		return ptr;
	}
};
