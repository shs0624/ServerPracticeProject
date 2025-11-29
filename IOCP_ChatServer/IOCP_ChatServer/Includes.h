#pragma once
#pragma comment(lib,"ws2_32")
#pragma comment(lib,"winmm.lib")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <Windows.h>
#include <crtdbg.h>
#include <minidumpapiset.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <conio.h>

#include "CFreeList.h"
#include "ProcademyProfiler.h"
#include "CSerializationBuffer.h"
#include "RefCountPointer.h"
#include "CRingBuffer.h"
#include "LockFreeQueue.h"
#include "LockFreeStack_Re.h"

#include "CCrashDump.h"
#include "DebugLog.h"



#define IOCP_THREADCOUNT 10
#define LOGCOUNT 10000