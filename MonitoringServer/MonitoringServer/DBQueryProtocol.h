#pragma once
#define QUERY_MAXCLASSSIZE 180
#include "time.h"
using std::ostringstream;

enum enQueryType
{
	_enLogin
};

struct CDBPoolStruct
{
	char _chMemory[QUERY_MAXCLASSSIZE];
};

class IDBJob
{
public:
	virtual bool Exec(ostringstream& oss) = 0;
};

class CDBLogin : public IDBJob
{
public:
	bool Exec(ostringstream& oss)
	{
		// 孽府 积己
		oss.clear();

		oss << "SELECT * FROM accountdb.sessionkey WHERE accountno = '"
			<< _AccountNum << "';";

		return true;
	}

	_int64 _AccountNum;
	char _SessionKey[64];
};

class CDBMonitorInsert : public IDBJob
{
public:
	bool Exec(ostringstream& oss)
	{
		// 孽府 积己
		oss.clear();

		oss << "INSERT into logdb." << _TableName << " values (NULL,'"
			<< _NowTime.tm_year + 1900 << "-" << _NowTime.tm_mon + 1 << "-" << _NowTime.tm_mday << " "
			<< _NowTime.tm_hour << ":" << _NowTime.tm_min << ":" << _NowTime.tm_sec << "',"
			<< _ServerNum << "," << _MonitorType << ", " << _Avr << ", " << _Min << ", " << _Max << ");";

		return true;
	}

	int _ServerNum;
	tm _NowTime;
	char _TableName[32];
	int _MonitorType;
	int _Avr;
	int _Max;
	int _Min;
};

class CDBCreateMonitorTable : public IDBJob
{
public:
	bool Exec(ostringstream& oss)
	{
		// 孽府 积己
		oss.clear();

		int month = _NowTime.tm_mon + 1;
		if (month < 10)
			oss << "CREATE TABLE IF NOT EXISTS logdb.monitorlog_" << _NowTime.tm_year + 1900 << "0" << month
			<< " LIKE monitorlog_template";
		else
			oss << "CREATE TABLE IF NOT EXISTS logdb.monitorlog_" << _NowTime.tm_year + 1900 << month
			<< " LIKE monitorlog_template";
		

		return true;
	}

	tm _NowTime;
};


