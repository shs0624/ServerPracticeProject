/*---------------------------------------------------------------

	procademy MemoryPool.

	메모리 풀 클래스 (오브젝트 풀 / 프리리스트)
	특정 데이타(구조체,클래스,변수)를 일정량 할당 후 나눠쓴다.

	- 사용법.

	procademy::CMemoryPool<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData 사용

	MemPool.Free(pData);


----------------------------------------------------------------*/
#ifndef  __PROCADEMY_MEMORY_POOL__
#define  __PROCADEMY_MEMORY_POOL__
#define DEFAULTSIZE 500
#include <new.h>
#include <Windows.h>

namespace procademy
{
	template <class DATA>
	class CMemoryPool
	{
		struct st_BLOCK_NODE
		{
			void* guardCode;
			DATA allocPtr;
			st_BLOCK_NODE* nextPtr;
		};
	public:
		//////////////////////////////////////////////////////////////////////////
		// 생성자, 파괴자.
		//
		// Parameters:	(int) 초기 블럭 개수.
		//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
		//				(bool) malloc 시 생성자 / Free 시 파괴자 호출 여부
		// Return:
		//////////////////////////////////////////////////////////////////////////
		CMemoryPool() {}

		CMemoryPool(int iBlockNum = 0, bool bPlacementNew = false, bool bCreateNew = false)
		{
			m_iCreateCount = (iBlockNum == 0) ? DEFAULTSIZE : iBlockNum;
			m_iCapacity = iBlockNum;
			m_iUseCount = 0;
			m_bPlacementNew = bPlacementNew;
			m_bCreateNew = bCreateNew;

			m_guardCode = (void*)this;
			_pFreeNode = nullptr;

			if (m_iCapacity == 0) return;

			for (int i = 0; i < iBlockNum; i++)
			{
				st_BLOCK_NODE* node = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));

				if (bCreateNew)
				{
					DATA* data;
					data = new(&(node->allocPtr)) DATA;
				}

				node->guardCode = m_guardCode;
				node->nextPtr = _pFreeNode;
				_pFreeNode = node;
			}
		}

		virtual	~CMemoryPool()
		{
			st_BLOCK_NODE* node = _pFreeNode;

			int subCount = (m_iCapacity - m_iUseCount);
			for (int i = 0; i < subCount; i++)
			{
				st_BLOCK_NODE* next = _pFreeNode->nextPtr;
				free(_pFreeNode);
				_pFreeNode = next;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// 블럭 하나를 할당받는다.  
		//
		// Parameters: 없음.
		// Return: (DATA *) 데이타 블럭 포인터.
		//////////////////////////////////////////////////////////////////////////
		DATA* Alloc(void)
		{
			// 호출했으니까, 데이터를 반환할 때까지 루프
			while (1)
			{
				if (m_iUseCount == m_iCapacity)
					Resize();

				st_BLOCK_NODE* oldTopNode = (st_BLOCK_NODE*)(0x00007fffffffffff & (ULONGLONG)_pFreeNode);
				st_BLOCK_NODE* newNode = oldTopNode->nextPtr;

#ifdef __GUARDTEST__
				oldTopNode->nextPtr = (st_BLOCK_NODE*)m_guardCode;
#endif

				if (InterlockedCompareExchange64((__int64*)&_pFreeNode, (__int64)newNode, (__int64)oldTopNode) == (__int64)oldTopNode)
				{
					// 바뀌었다!
					DATA* data = &(oldTopNode->allocPtr);
					if (m_bPlacementNew)
					{
						data = new(data) DATA;
					}

					InterlockedIncrement(&m_iUseCount);

					return data;
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// 사용중이던 블럭을 해제한다.
		//
		// Parameters: (DATA *) 블럭 포인터.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		bool Free(DATA* pData)
		{
			st_BLOCK_NODE* newNode = (st_BLOCK_NODE*)((char*)pData - 8);

#ifdef __GUARDTEST__
			if (newNode->guardCode != m_guardCode || newNode->nextPtr != m_guardCode)
			{
				DebugBreak();
			}
#endif

			while (1)
			{
				ULONGLONG localIdx = _IDCnt;
				st_BLOCK_NODE* oldTopNode = _pFreeNode;
				newNode->nextPtr = oldTopNode;

				localIdx = 0x000000000001ffff & localIdx;
				localIdx = localIdx << 47;
				newNode = (st_BLOCK_NODE*)((ULONGLONG)newNode | localIdx);

				if (InterlockedCompareExchange64((__int64*)&_pFreeNode, (__int64)newNode, (__int64)oldTopNode) == (__int64)oldTopNode)
				{
					// 바뀌었다. 반환해야지.
					if (m_bPlacementNew || m_bCreateNew)
					{
						newNode->allocPtr.~DATA();
					}

					InterlockedDecrement(&m_iUseCount);
					return true;
				}
			}

			return false;
		}


		//////////////////////////////////////////////////////////////////////////
		// 현재 확보 된 블럭 개수를 얻는다. (메모리풀 내부의 전체 개수)
		//
		// Parameters: 없음.
		// Return: (int) 메모리 풀 내부 전체 개수
		//////////////////////////////////////////////////////////////////////////
		int		GetCapacityCount(void) { return m_iCapacity; }

		//////////////////////////////////////////////////////////////////////////
		// 현재 사용중인 블럭 개수를 얻는다.
		//
		// Parameters: 없음.
		// Return: (int) 사용중인 블럭 개수.
		//////////////////////////////////////////////////////////////////////////
		int		GetUseCount(void) { return m_iUseCount; }


		// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리. - 스택의 탑 포인터.
		st_BLOCK_NODE* _pFreeNode;
	private:
		void Resize(void)
		{
			for (int i = 0; i < m_iCreateCount; i++)
			{
				st_BLOCK_NODE* node = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));

				if (m_bCreateNew)
				{
					DATA* data;
					data = new(&(node->allocPtr)) DATA;
				}

				node->guardCode = m_guardCode;
				node->nextPtr = _pFreeNode;
				_pFreeNode = node;
			}

			m_iCapacity += m_iCreateCount;
		}

		ULONGLONG _IDCnt = 1;

		int m_iCreateCount;
		int m_iCapacity;
		unsigned int m_iUseCount;
		bool m_bPlacementNew;
		bool m_bCreateNew;
		void* m_guardCode;
	};
}
#endif