#define DEFAULTSIZE 10000
template <typename T>
class TestStack
{
public:
	TestStack()
	{
		_pStack = (T*)malloc(sizeof(T) * DEFAULTSIZE);
		idx = 0;
	}

	TestStack(int& max)
	{
		_pStack = (T*)malloc(sizeof(T) * max);
		idx = 0;
	}

	~TestStack()
	{
		free(_pStack);
	}

	void push(T& data)
	{
		_pStack[idx++] = data;
	}

	void pop()
	{
		idx--;
	}

	int count()
	{
		return idx;
	}

	T& top()
	{
		return _pStack[idx - 1];
	}
private:
	T* _pStack;
	int idx;
};