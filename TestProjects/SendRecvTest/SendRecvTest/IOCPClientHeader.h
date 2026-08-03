#pragma once
#define SERVERIP "127.0.0.1"
#define SERVERPORT 9000
#define BUFSIZE 512

unsigned int WINAPI LogThread(LPVOID arg);

// 오류 출력 함수
void err_quit(const char* msg);
void err_display(const char* msg);