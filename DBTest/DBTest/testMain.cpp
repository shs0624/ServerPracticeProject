#pragma comment(lib,"libmysql.lib")
#include <iostream>
#include <Windows.h>
#include <thread>
#include <sstream>
#include <string>
#include <queue>
#include "mysql.h"
#include "errmsg.h"
#include "LogManager.h"
using namespace std;

struct st_DBQUERY
{
	DWORD num1;
	DWORD num2;
	DWORD num3;
};

HANDLE g_hEvent;
SRWLOCK _queueLock;
std::queue<st_DBQUERY*> queryQueue;
int _iQueueSize;

MYSQL conn;
MYSQL* connection;

void UpdateThread();
void DBWriterThread();

LONG num = 0;

int main()
{
	MYSQL_RES* sql_result;
	MYSQL_ROW sql_row;
	st_DBQUERY* pQuery;
	int query_stat;

	connection = NULL;

	InitializeSRWLock(&_queueLock);

	// 초기화
	mysql_init(&conn);
	g_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// DB 연결
	connection = mysql_real_connect(&conn, "127.0.0.1", "root", "shs0624@@", "newschema", 3306, (char*)NULL, CLIENT_MULTI_STATEMENTS);
	if (connection == NULL)
	{
		fprintf(stderr, "Mysql connection error : %s", mysql_error(&conn));
		return 1;
	}

	std::ostringstream oss;
	oss << "TRUNCATE `newschema`.`newtable`";
	std::string sql = oss.str();
	// Select 쿼리문
	query_stat = mysql_query(connection, sql.c_str());
	if (query_stat != 0)
	{
		printf("Mysql query error : %s", mysql_error(&conn));
		return 0;
	}
	sql_result = mysql_store_result(connection);
	mysql_free_result(sql_result);

	std::thread writerThread(DBWriterThread);

	std::thread updateThread[2];
	for (int i = 0; i < 2; i++)
	{
		updateThread[i] = std::thread(UpdateThread);
	}

	writerThread.join();
	updateThread[0].join();
	updateThread[1].join();

	// DB 연결닫기
	mysql_close(connection);
}

void UpdateThread()
{
	thread_local stChatLog _pLog;
	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	while (1)
	{
		st_DBQUERY* pQuery = new st_DBQUERY;

		pQuery->num1 = InterlockedAdd(&num, 3);
		pQuery->num2 = pQuery->num1 + 10;
		pQuery->num3 = pQuery->num1 + 100;

		AcquireSRWLockExclusive(&_queueLock);
		queryQueue.push(pQuery);
		ReleaseSRWLockExclusive(&_queueLock);

		_pLog._lQueueSize++;
		SetEvent(g_hEvent);

		//Sleep(0);
	}
}

void DBWriterThread()
{
	thread_local stChatLog _pLog;
	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	MYSQL_RES* sql_result;
	MYSQL_ROW sql_row;
	st_DBQUERY* pQuery;
	int query_stat;

	while (1)
	{
		WaitForSingleObject(g_hEvent,INFINITE);

		AcquireSRWLockExclusive(&_queueLock);
		pQuery = queryQueue.front();
		queryQueue.pop();

		_pLog._lQueueSize--;
		ReleaseSRWLockExclusive(&_queueLock);

		//std::ostringstream oss;
		//oss.clear();
		//oss << "INSERT INTO newschema.newtable VALUE ("
		//	<< pQuery->num1 << ","
		//	<< pQuery->num1 + 10 << ","
		//	<< pQuery->num1 + 100 << ")";

		//std::string sql = oss.str();
		//// Select 쿼리문
		//query_stat = mysql_query(connection, sql.c_str());
		//if (query_stat != 0)
		//{
		//	printf("Mysql query error : %s", mysql_error(&conn));
		//	return;
		//}

		//for (int i = 0; i < 3; i++)
		//{
		//	std::ostringstream oss;
		//	int n = pQuery->num1 + i;
		//	oss.clear();
		//	oss << "INSERT INTO newschema.newtable VALUE ("
		//		<< n << ","
		//		<< n + 10 << ","
		//		<< n + 100 << ")";

		//	std::string sql = oss.str();
		//	// Select 쿼리문
		//	query_stat = mysql_query(connection, sql.c_str());
		//	if (query_stat != 0)
		//	{
		//		printf("Mysql query error : %s", mysql_error(&conn));
		//		return;
		//	}

		// 트랜잭션
			//// 결과출력
			//sql_result = mysql_store_result(connection);		// 결과 전체를 미리 가져옴
			//mysql_free_result(sql_result);

		// 쿼리 여러번
		//std::ostringstream ossOut;
		//ossOut << "BEGIN;\n";
		//std::string sql = ossOut.str();
		//// Select 쿼리문
		//query_stat = mysql_query(connection, sql.c_str());
		//if (query_stat != 0)
		//{
		//	printf("Mysql query error : %s", mysql_error(&conn));
		//	return;
		//}

		//for (int i = 0; i < 3; i++)
		//{
		//	std::ostringstream oss;
		//	int n = pQuery->num1 + i;
		//	oss.clear();
		//	oss << "INSERT INTO newschema.newtable VALUE ("
		//		<< n << ","
		//		<< n + 10 << ","
		//		<< n + 100 << ")";

		//	std::string sql = oss.str();
		//	// Select 쿼리문
		//	query_stat = mysql_query(connection, sql.c_str());
		//	if (query_stat != 0)
		//	{
		//		printf("Mysql query error : %s", mysql_error(&conn));
		//		return;
		//	}	
		//}

		//ossOut.clear();
		//ossOut.str("");
		//ossOut << "COMMIT;\n";
		//std::string sql2 = ossOut.str();
		//// Select 쿼리문
		//query_stat = mysql_query(connection, sql2.c_str());
		//if (query_stat != 0)
		//{
		//	printf("Mysql query error : %s", mysql_error(&conn));
		//	return;
		//}

		std::ostringstream oss;
		oss.clear();

		oss << "BEGIN;\n";
		oss << "INSERT INTO newschema.newtable VALUE ("
			<< pQuery->num1 << ","
			<< pQuery->num1 + 10 << ","
			<< pQuery->num1 + 100 << ");\n";
		pQuery->num1++;
		oss << "INSERT INTO newschema.newtable VALUE ("
			<< pQuery->num1 << ","
			<< pQuery->num1 + 10 << ","
			<< pQuery->num1 + 100 << ");\n";
		pQuery->num1++;
		oss << "INSERT INTO newschema.newtable VALUE ("
			<< pQuery->num1 << ","
			<< pQuery->num1 + 10 << ","
			<< pQuery->num1 + 100 << ");\n";
		oss << "COMMIT;\n";

		std::string sql = oss.str();
		// Select 쿼리문
		query_stat = mysql_query(connection, sql.c_str());
		if (query_stat != 0) {
			printf("Mysql query error : %s", mysql_error(&conn));
			return;
		}

		// 멀티문 결과 비우기
		int status = 0;
		do {
			MYSQL_RES* res = mysql_store_result(connection); // INSERT/COMMIT은 NULL이어도 OK
			if (res) mysql_free_result(res);
			status = mysql_next_result(connection);
		} while (status == 0);

		if (status > 0) { // -1이 아닌 경우 에러
			printf("Mysql multi-result error : %s", mysql_error(&conn));
			return;
		}

		_pLog._lInsertTPS++;
		delete(pQuery);
	}
}