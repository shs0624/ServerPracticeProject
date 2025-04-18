#include "CRingBuffer.h"

CRingBuffer::CRingBuffer()
{
	arr = (char*)malloc(sizeof(char) * DEFAULTSIZE + (1 * sizeof(char)));
	if (arr == NULL)
	{
		DebugBreak();
	}

	head = 0;
	tail = 0;
	max = DEFAULTSIZE + 1;
}

CRingBuffer::CRingBuffer(int size)
{
	arr = (char*)malloc(sizeof(char) * size + 1 * sizeof(char));
	if (arr == NULL)
	{
		DebugBreak();
	}

	head = 0;
	tail = 0;
	max = size + 1;
}

CRingBuffer::~CRingBuffer(void)
{
	free(arr);
}

int CRingBuffer::GetBufferSize()
{
	return max - 1;
}

int CRingBuffer::GetFreeSize()
{
	return max - ((tail - head) + max);
}

int CRingBuffer::GetUseSize()
{
	return (tail - head) + max;
}

int CRingBuffer::Enqueue(char* input, int size)
{
	// 넣을 수 있는 사이즈 얻기
	int freeSize = GetFreeSize();
	int enqueueSize = (freeSize >= size) ? size : freeSize;

	memcpy((void*)arr[tail], (void*)input, enqueueSize);
	tail = (tail + enqueueSize) % max;
	return enqueueSize;
}

int CRingBuffer::Dequeue(char** output, int size)
{
	// 뺄 수 있는 사이즈 얻기
	int useSize = GetUseSize();
	int dequeueSize = (useSize < size) ? useSize : size;

	memcpy((void*)output, (void*)arr[head], dequeueSize);
	head = (head + dequeueSize) % max;
	return dequeueSize;
}

int CRingBuffer::Peek(char** output, int size)
{
	// 뺄 수 있는 사이즈 얻기
	int useSize = GetUseSize();
	int dequeueSize = (useSize < size) ? useSize : size;

	memcpy((void*)output, (void*)arr[head], dequeueSize);
	return dequeueSize;
}

void CRingBuffer::ClearBuffer(void)
{
	head = 0;
	tail = 0;
}