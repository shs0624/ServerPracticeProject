#pragma once
#include "CSerializationBuffer.h"
#include "ProcademyProfiler.h"
#include <Windows.h>

template <typename T>
class RefCountPointer
{
public:
	RefCountPointer()
	{

	}

	/*~RefCountPointer()
	{
		if (InterlockedDecrement(_iRefCount) == 0)
		{
			delete(_iRefCount);
			CPacket::_CPacketPool.Free(ptr);
		}
	}*/

	static RefCountPointer<T> MakeSharedPtr()
	{
		RefCountPointer<T> result;
		result._iRefCount = new unsigned int(0);
		{
			//Profiler("Alloc");
			result.ptr = CPacket::_CPacketPool.Alloc();
		}

		return result;
	}

	/*
	static RefCountPointer<T> MakeSharedPtr(bool isAuto)
	{
		RefCountPointer<T> result;
		result._iRefCount = new unsigned int(1);
		result.ptr = new T;
		result._isAuto = isAuto;

		return result;
	}

	static RefCountPointer<T> MakeSharedPtr(bool isAuto, int arg1)
	{
		RefCountPointer<T> result;
		result._iRefCount = new unsigned int(1);
		result.ptr = new T(arg1);
		result._isAuto = isAuto;

		return result;
	}

	static RefCountPointer<T> MakeSharedPtr(bool isAuto, int arg1, int arg2)
	{
		RefCountPointer<T> result;
		result._iRefCount = new unsigned int(1);
		result.ptr = new T(arg1, arg2);
		result._isAuto = isAuto;

		return result;
	}
	*/

	T* operator*()
	{
		//현재 노드의 데이터를 뽑음
		return ptr;
	}

	/*RefCountPointer<T>& operator= (const RefCountPointer<T>& copy)
	{
		ptr = copy.ptr;
		_iRefCount = copy._iRefCount;

		InterlockedIncrement((LONG*)_iRefCount);

		return *this;
	}

	RefCountPointer(const RefCountPointer<T>& copy)
	{
		ptr = copy.ptr;
		_iRefCount = copy._iRefCount;

		InterlockedIncrement((LONG*)_iRefCount);
	}*/
private:
	T* ptr;
	unsigned int* _iRefCount;

	void IncRefCount()
	{
		InterlockedIncrement((LONG*)_iRefCount);
	}

	void DecRefCount()
	{
		if (InterlockedDecrement((LONG*)_iRefCount) == 0)
		{
			//Profiler("Free");
			delete(_iRefCount);
			CPacket::_CPacketPool.Free(ptr);
		}
	}

	friend class CLanServer;
};