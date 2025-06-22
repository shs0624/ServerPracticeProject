#define DEFAULTSIZE 10000
template <typename T>
class CStack
{
public:
	CStack()
	{
		_pStack = (T*)malloc(sizeof(T) * DEFAULTSIZE);
		idx = 0;
	}

	CStack(int max)
	{
		_pStack = (T*)malloc(sizeof(T) * max);
		idx = 0;
	}

	~CStack()
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

	bool empty()
	{
		if (idx == 0)
			return true;
		else
			return false;
	}

	void clear()
	{
		idx = 0;
	}

	T& top()
	{
		return _pStack[idx - 1];
	}
private:
	T* _pStack;
	int idx;
};