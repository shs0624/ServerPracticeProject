#include "CRingBuffer.h"

CRingBuffer::CRingBuffer()
{
	arr = (char*)malloc(DEFAULTSIZE + 1);
	if (arr == NULL)
	{
		DebugBreak();
	}

	head = 0;
	tail = 0;
	count = 0;
	max = DEFAULTSIZE;
}

CRingBuffer::CRingBuffer(int size)
{
	arr = (char*)malloc(size + 1);
	if (arr == NULL)
	{
		DebugBreak();
	}

	head = 0;
	tail = 0;
	count = 0;
	max = size;
}

CRingBuffer::~CRingBuffer(void)
{
	free(arr);
}

int CRingBuffer::GetBufferSize()
{
	return max;
}

int CRingBuffer::GetFreeSize()
{
	return max - count;
}

int CRingBuffer::GetUseSize()
{
	return count;
}

int CRingBuffer::Enqueue(char* input, int size)
{
	// 넣을 수 있는 사이즈 얻기
	int freeSize = GetFreeSize();
	int enqueueSize = (freeSize >= size) ? size : freeSize;

	if (tail + enqueueSize > max)
	{
		// 경계를 넘는다면
		int cutSize = max - tail;
		memcpy((void*)(arr + tail), (void*)input, cutSize);
		memcpy((void*)arr, (void*)(input + cutSize), enqueueSize - cutSize);
	}
	else
	{
		memcpy((void*)(arr + tail), (void*)input, enqueueSize);
	}

	tail = (tail + enqueueSize) % max;
	count += enqueueSize;
	return enqueueSize;
}

int CRingBuffer::Dequeue(char* output, int size)
{
	// 뺄 수 있는 사이즈 얻기
	int useSize = GetUseSize();
	int dequeueSize = (useSize < size) ? useSize : size;

	if (head + dequeueSize > max)
	{
		// 경계를 넘는다면
		int cutSize = max - head;
		memcpy((void*)output, (void*)(arr + head), cutSize);
		memcpy((void*)(output + cutSize), (void*)arr, dequeueSize - cutSize);
	}
	else
	{
		memcpy((void*)output, (void*)(arr + head), dequeueSize);
	}

	head = (head + dequeueSize) % max;
	count -= dequeueSize;
	return dequeueSize;
}

int CRingBuffer::Peek(char* output, int size)
{
	// 뺄 수 있는 사이즈 얻기
	int useSize = GetUseSize();
	int dequeueSize = (useSize < size) ? useSize : size;

	if (head + dequeueSize > max)
	{
		// 경계를 넘는다면
		int cutSize = max - head;
		memcpy((void*)output, (void*)(arr + head), cutSize);
		memcpy((void*)(output + cutSize), (void*)arr, dequeueSize - cutSize);
	}
	else
	{
		memcpy((void*)output, (void*)(arr + head), dequeueSize);
	}

	return dequeueSize;
}

int CRingBuffer::DirectEnqueueSize(void)
{
	return max - tail;
}

int CRingBuffer::DirectDequeueSize(void)
{
	return max - head;
}

// 호출 전에 FreeSize를 체크하고 넣을거다.
int CRingBuffer::MoveRear(int iSize)
{
	tail += iSize;
	tail = tail % max;
	count += iSize;
	return iSize;
}

// 호출 전에 UseSize를 체크하고 넣을거다.
int CRingBuffer::MoveFront(int iSize)
{
	head += iSize;
	head = head % max;
	count -= iSize;
	return iSize;
}

char* CRingBuffer::GetFrontBufferPtr(void)
{
	return (arr + head);
}

char* CRingBuffer::GetRearBufferPtr(void)
{
	return (arr + tail);
}

char* CRingBuffer::GetArrPtr(void)
{
	return arr;
}

void CRingBuffer::ClearBuffer(void)
{
	head = 0;
	tail = 0;
	count = 0;
}