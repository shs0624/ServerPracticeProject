#pragma once
#include <Windows.h>

template <typename T>
class RefCountPointer
{
public:
	RefCountPointer()
	{

	}

	~RefCountPointer()
	{
		if (InterlockedDecrement(_iRefCount) == 0)
		{
			delete(ptr);
		}
	}

	static RefCountPointer<T> MakeSharedPtr(bool isAuto)
	{
		RefCountPointer<T> result;
		result._iRefCount = new unsigned int(1);
		result.ptr = new T;
		result._isAuto = isAuto;

		return result;
	}

	T* operator*()
	{
		//현재 노드의 데이터를 뽑음
		return ptr;
	}

	RefCountPointer<T>& operator= (const RefCountPointer<T>& copy)
	{
		this->ptr = copy.ptr;
		this->_iRefCount = copy.ptr;
		this->_isAuto = copy._isAuto;

		if (_isAuto)
		{
			InterlockedIncrement((LONG*)_iRefCount);
		}

		return *this;
	}

	RefCountPointer(const RefCountPointer<T>& copy)
	{
		ptr = copy.ptr;
		_iRefCount = copy._iRefCount;
		_isAuto = copy._isAuto;

		if (_isAuto)
		{
			InterlockedIncrement((LONG*)_iRefCount);
		}
	}

	void IncRefCount()
	{
		InterlockedIncrement((LONG*)_iRefCount);
	}

	void DecRefCount()
	{
		if (InterlockedDecrement((LONG*)_iRefCount) == 0)
		{
			delete(ptr);
		}
	}
private:
	T* ptr;
	unsigned int* _iRefCount;
	bool _isAuto;
};