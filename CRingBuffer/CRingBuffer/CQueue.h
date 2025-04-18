#pragma once
template<typename T>
class CQueue
{
public:
	CQueue(int size)
	{
		arr = (T*)malloc(sizeof(T) * size);
	}

	~CQueue()
	{
		free(arr);
	}

	bool Enqueue(T input)
	{

	}

	bool Dequeue(T* output)
	{

	}

	bool Peek(T* output)
	{

	}
private:
	T* arr;
	int front = 0;
	int rear = 0;
};