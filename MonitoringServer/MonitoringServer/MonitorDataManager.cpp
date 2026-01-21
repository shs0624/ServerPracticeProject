#include "Includes.h"
#include "MonitorProtocol.h"
#include "NetServer.h"
#include "LanServer.h"
#include "LogManager.h"
#include "MonitorClientServer.h"
#include "MonitorDataManager.h"
#include "MonitorChatServer.h"

thread_local stChatLog MonitorDataManager::_pLog;

TLSMemoryPoolManager<CDBPoolStruct>
SHS::DBTLSConnector::_JobPool(1000, 5, 10);

TLSMemoryPoolManager<CDBPoolStruct>
SHS::DBWriterManager::_JobPool(1000, 5, 10);