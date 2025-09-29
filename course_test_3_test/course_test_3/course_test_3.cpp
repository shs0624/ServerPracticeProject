// ConsoleApplication612.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <process.h>
#include <queue>
#include <string>
#include <chrono>
#include <cmath>

#include "sha512.h"
#include "sha512_check_lib.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "sha512_check_lib.lib")

struct stMESSAGE_REQ
{
	unsigned __int64	key;
	std::string			data;
};

struct stMESSAGE_RES
{
	unsigned __int64	key;
	std::string			sha512;
};


std::queue<stMESSAGE_REQ*> g_msgQueue;
char g_msgQueueFlag = 0;
SRWLOCK g_msgQueue_lock;

std::queue<stMESSAGE_RES*> g_hashQueue;
SRWLOCK g_hashQueue_lock;


unsigned __int64 g_keyGenerator = 0;

// 랜덤한 문자열 생성 함수
char text_table[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
					'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '1', '2', '3', '4', 
					'5', '6', '7', '8', '9', '0', '!', '@', '#', '$', '%', '^', '&', '*', '(' };

void MakeString(std::string &str)
{
	str.clear();

	int len = 4 + (rand() % 10);
	while ( len-- > 0 )
	{
		str.append(1, text_table[rand() % 45]);
	}
}


void HashRequest(std::string &data)
{
	stMESSAGE_REQ* pmsg = new stMESSAGE_REQ;
	pmsg->data = data;
	pmsg->key = ++g_keyGenerator;

//------------------------------------------------------------------


	AcquireSRWLockExclusive(&g_msgQueue_lock);
	g_msgQueue.push(pmsg);
	ReleaseSRWLockExclusive(&g_msgQueue_lock);




}


void HashResponse(const std::string &hash, unsigned __int64 key)
{
	stMESSAGE_RES *res = new stMESSAGE_RES;
	res->sha512 = hash;
	res->key = key;

	AcquireSRWLockExclusive(&g_hashQueue_lock);
	g_hashQueue.push(res);
	ReleaseSRWLockExclusive(&g_hashQueue_lock);
}




unsigned int WINAPI HashThread(LPVOID lpParam)
{
	stMESSAGE_REQ* msg;

	while (1)
	{
		msg = nullptr;
		AcquireSRWLockExclusive(&g_msgQueue_lock);
		if ( !g_msgQueue.empty() )
		{
			msg = g_msgQueue.front();
			g_msgQueue.pop();
		}
		ReleaseSRWLockExclusive(&g_msgQueue_lock);

		if (msg == nullptr)
			continue;


		HashResponse(sha512(msg->data), msg->key);

		delete msg;
	}
}

unsigned int g_resTPS = 0;

unsigned int WINAPI CompleteThread(LPVOID lpParam)
{
	stMESSAGE_RES* res;
	unsigned int resCnt = 0;
	DWORD secTick = timeGetTime();

	while (1)
	{
		res = nullptr;
		AcquireSRWLockExclusive(&g_hashQueue_lock);
		if ( !g_hashQueue.empty() )
		{
			res = g_hashQueue.front();
			g_hashQueue.pop();
		}
		ReleaseSRWLockExclusive(&g_hashQueue_lock);

		if (res == nullptr)
		{
			Sleep(0);
			continue;
		}

		
		{
			// key 와 sha512 값으로 무언가를 한다고 함
			// 지금은 예시이므로 아무것도 안함

			res->key;
			res->sha512;

		}



		{
			resCnt++;
			DWORD nowTick = timeGetTime();
			if (nowTick - secTick >= 1000 && nowTick > secTick)
			{
				secTick += 1000;
				g_resTPS = resCnt;
				resCnt = 0;
			}
		}


		delete res;
	}
}


int main()
{
	timeBeginPeriod(1);
	srand((unsigned int)time(nullptr));
	unsigned long seed_round = rand();

	HANDLE hThread[5];
	
	InitializeSRWLock(&g_hashQueue_lock);
	InitializeSRWLock(&g_msgQueue_lock);
	init_prepare();
	
	/*
	
	프로그램의 목표 : 임의로 생성된 문자열 데이터를 빠르게 멀티 스레드로 SHA512 해시 하도록 한다.

	main thread

		임의의 문자열과 이에 맵핑될 키값을 생성
		이를 stMESSAGE_REQ 에 담아 g_msgQueue 에 넣어 HashThread 에게 요청

	hash thread x 3

		g_msgQueue 의 요청 메시지를 얻어 SHA512 를 진행
		완성된 해시를 g_hashQueue 에 넣어 complete thread 에게 전달 함



	complete thread

		해시작업이 완료되면 g_hashQueue 에 들어오며 이를 키와 맵핑하여 사용함 (실제로 사용하는 로직은 없음)



	*/



	hThread[0] = (HANDLE)_beginthreadex(NULL, 0, HashThread, (LPVOID)0, 0, nullptr);
	hThread[1] = (HANDLE)_beginthreadex(NULL, 0, HashThread, (LPVOID)0, 0, nullptr);
	hThread[2] = (HANDLE)_beginthreadex(NULL, 0, CompleteThread, (LPVOID)0, 0, nullptr);

	unsigned int reqCnt = 0;
	std::string originalString;
	DWORD secTick = timeGetTime();




	while (1)
	{
		// 임의의 문자열이 생성되고, 이를 해시요청 함
		MakeString(originalString);
		HashRequest(originalString);

		{ // 속도 조절을 위한 대기 코드
			Sleep(0);
			Sleep(0);
			Sleep(0);
			Sleep(0);
			Sleep(0);
			Sleep(0);
 			Sleep(0);
		}

		reqCnt++;

		{ // 1초마다 요청수 출력을 위한 코드

			DWORD nowTick = timeGetTime();
			if (nowTick - secTick >= 1000 && nowTick > secTick)
			{
				// g_resTPS 의 1초 정산은 Complete Thread 에서 되므로 완벽한 정산 동기화가 안되어 같은 값이 나올 수 있음
				// 이는 무시하고 1초마다 출력을 목적으로 여기서 한번에 하도록 함

				std::cout << "request msg : " << reqCnt / 1000 << "k / sec | complete msg : " << g_resTPS / 1000 << "k / sec | queue size : " << g_msgQueue.size() << std::endl;
				secTick += 1000;
				reqCnt = 0;
			}
		}
	}

	// 본 프로그램은 종료 조건이 없음 영원히 돌 것임
    
}
