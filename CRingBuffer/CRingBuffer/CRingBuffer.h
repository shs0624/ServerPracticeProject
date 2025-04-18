#pragma once
#include <iostream>
#include <Windows.h>
#define DEFAULTSIZE 1024

class CRingBuffer
{
public:
	CRingBuffer();

	CRingBuffer(int size);

	~CRingBuffer(void);

	void Resize(int size);

	// 버퍼 사이즈 얻기
	int GetBufferSize(void);
	// 사용중인 용량 얻기
	int GetUseSize(void);
	// 버퍼에 남은 용량 얻기
	int GetFreeSize(void);

	// 데이터 넣고 넣은 크기 반환, end 이동
	int Enqueue(char* input, int size);

	// 앞에서 데이터 빼고 가져온 크기 반환, front 이동
	int Dequeue(char** output, int size);

	// 앞에서 데이터 빼고 가져온 크기 반환, front 이동 X
	int Peek(char** output, int size);

	// 버퍼 비우기 -> front, rear만 조정해서 데이터 밀지 않기
	void ClearBuffer(void);
private:
	char* arr;
	int head;
	int tail;
	int max;
};