#include <iostream>
#include <Windows.h>
#include <string>

std::string GenerateSessionKey()
{
	srand(time(NULL));
	static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const size_t KEY_LEN = 64;
	BYTE randomBytes[KEY_LEN];

	std::string sessionKey;

	int len = alphabet.size();
	for (int i = 0; i < KEY_LEN; i++)
	{
		int randNum = (rand() % len);
		sessionKey.push_back(alphabet[randNum]);
	}

	return sessionKey;
}

int main()
{
	GenerateSessionKey();

	return 0;
}