#pragma once
#pragma comment(lib, "ws2_32")
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <process.h>
#include <conio.h>
#include <cstdio>
#include <random>
#include <io.h>
#include <time.h>
#include <fcntl.h>
#include <algorithm>
#include <crtdbg.h>
#include <minidumpapiset.h>
#include <list>
#include <vector>
#include <string>
#include <unordered_map>

#include "CCrashDump.h"
#include "RefCountPointer.h"
#include "DebugLog.h"
#include "LockFreeQueue.h"
#include "CRingBuffer.h"