#pragma once
#define DEFAULTPOOLSIZE 50
#include <new.h>
#include <Windows.h>

// 락프리 메모리풀 -> 스택 구조로 되어있음
namespace NetLib
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
		CMemoryPool() {}

		// (초기 블럭 개수, Alloc시 생성자 호출 여부, 메모리 할당시 생성자 호출 여부)
		CMemoryPool(int iBlockNum = 0, bool bPlacementNew = false, bool bCreateNew = false)
		{
			m_iCreateCount = (iBlockNum == 0) ? DEFAULTPOOLSIZE : iBlockNum;
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

		// 할당
		DATA* Alloc(void)
		{
			if (m_iUseCount == m_iCapacity)
				Resize();

			st_BLOCK_NODE* allocNode = _pFreeNode;
			_pFreeNode = _pFreeNode->nextPtr;

#ifdef __GUARDTEST__
			allocNode->nextPtr = (st_BLOCK_NODE*)m_guardCode;
#endif

			DATA* data = &(allocNode->allocPtr);
			if (m_bPlacementNew)
			{
				data = new(data) DATA;
			}

			++m_iUseCount;

			return data;
		}

		// 반환
		bool Free(DATA* pData)
		{
			st_BLOCK_NODE* ptr = (st_BLOCK_NODE*)((char*)pData - 8);

#ifdef __GUARDTEST__
			if (ptr->guardCode != m_guardCode || ptr->nextPtr != m_guardCode)
			{
				DebugBreak();
			}
#endif

			if (m_bPlacementNew || m_bCreateNew)
			{
				ptr->allocPtr.~DATA();
			}

			ptr->nextPtr = _pFreeNode;
			_pFreeNode = ptr;
			m_iUseCount--;

			return true;
		}


		// 남은 노드 개수
		int		GetCapacityCount(void) { return m_iCapacity; }

		// 사용중인 노드 개수
		int		GetUseCount(void) { return m_iUseCount; }


		// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리. - 스택의 탑 포인터.
		st_BLOCK_NODE* _pFreeNode;
	private:
		void Resize(void)
		{
			DebugBreak();
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

		int m_iCreateCount;
		int m_iCapacity;
		int m_iUseCount;
		bool m_bPlacementNew;
		bool m_bCreateNew;
		void* m_guardCode;
	};
}