#pragma once
#include <Windows.h>
// 메인 메모리 풀은 청크 단위로 오브젝트들을 관리하는 풀이다. 
// 락프리 구조로 구현.
#define ALLOCCOUNT 2
#define MAXCAPACITY_CHUNK 7


struct stChunk
{
	LPVOID BottomNode;
	LPVOID TopNode;
};

template <typename DATA>
class TLSMemoryPoolManager
{
	template <typename T>
	struct st_BLOCK_NODE
	{
		void* guardCode;
		T allocData;
		st_BLOCK_NODE<T>* nextPtr;
	};

	friend class TLSMemoryPool;
public:
	// 매개변수 (1청크에 들어가는 노드 개수, 스레드에 할당할 기본 청크 개수, 스레드 개수, 할당 받을때 생성자 호출 여부, 생성 때 생성자 호출 여부)
	TLSMemoryPoolManager(unsigned int iChunkSize = 0, unsigned int iChunkPerThread = 0, unsigned int iThreadCount = 0,
		bool bPlacementNew = false, bool bCreateNew = false)
	{
		_TlsIdx = TlsAlloc();
		_ThreadCount = 0;

		_iChunkSize = iChunkSize;
		_iChunkPerThread = iChunkPerThread;

		// 총 생성 청크는 전체 스레드의 요구 청크 * 2만큼
		_iCreateChunkCount = iThreadCount * iChunkPerThread * 2;
		_iLeftChunk = _iCreateChunkCount;

		_bPlacementNew = bPlacementNew;
		_bCreateNew = bCreateNew;

		_TopNode = nullptr;

		_pGuardCode = (LPVOID)this;

		CreateChunk();
	}

	// 스레드 별 스택 생성
	void Thread_Init()
	{
		if (_TlsIdx == 0)
			DebugBreak();

		// 스레드의 메모리풀 주소 얻어오기
		TLSMemoryPool* pMemoryPool = new TLSMemoryPool(_iChunkPerThread, MAXCAPACITY_CHUNK, this);
		TlsSetValue(_TlsIdx, (LPVOID)pMemoryPool);

		// 여기에 내가 생성해놓은 노드들 단체로 이동
		//for (int i = 0; i < _iChunkPerThread; i++)
		//{
		//	// 청크를 뽑아서, 이걸 그 스레드에 전달.
		//	pMemoryPool->AllocChunkFromPool();
		//}
		pMemoryPool->AllocChunkFromPool();
	}

	// 종료할 때 동적할당 해제용도
	void Thread_CleanUp();

	// 청크 할당 메인 -> TLS
	stChunk* AllocChunkToTLS()
	{
		// 이거 다시 생성하는거로 바꿔야함
		if (_TopNode == NULL)
			CreateChunk();

		// 호출했으니까, 데이터를 반환할 때까지 루프
		while (1)
		{
			if (_TopNode == NULL)
				CreateChunk();

			st_BLOCK_NODE<stChunk>* oldTopNode = _TopNode;

			st_BLOCK_NODE<stChunk>* NodePtr = (st_BLOCK_NODE<stChunk>*)(0x00007fffffffffff & (ULONGLONG)oldTopNode);
			st_BLOCK_NODE<stChunk>* newNode = NodePtr->nextPtr;

#ifdef __GUARDTEST__
			NodePtr->nextPtr = (st_BLOCK_NODE*)m_guardCode;
#endif

			if (InterlockedCompareExchange64((__int64*)&_TopNode, (__int64)newNode, (__int64)oldTopNode) == (__int64)oldTopNode)
			{
				stChunk* data = &(NodePtr->allocData);

				/*DWORD localCnt = InterlockedIncrement(&_logIdx);
				_LogArr[localCnt].ptr = NodePtr;
				_LogArr[localCnt].type = ALLOC;*/

				InterlockedIncrement(&_iUseChunk);
				InterlockedDecrement(&_iLeftChunk);
				return data;
			}
		}
	}

	// 청크 해제 TLS -> 메인
	void FreeChunkToPool(stChunk* chunk)
	{
		st_BLOCK_NODE<stChunk>* nodePtr = (st_BLOCK_NODE<stChunk>*)((char*)chunk - sizeof(void*));

		while (1)
		{
			ULONGLONG localIdx = _ulIDCnt;
			st_BLOCK_NODE<stChunk>* oldTopNode = _TopNode;
			nodePtr->nextPtr = oldTopNode;

			localIdx = localIdx << 47;
			st_BLOCK_NODE<stChunk>* newNode = (st_BLOCK_NODE<stChunk>*)((ULONGLONG)nodePtr | localIdx);

			if (InterlockedCompareExchange64((__int64*)&_TopNode, (__int64)newNode, (__int64)oldTopNode) == (__int64)oldTopNode)
			{
				/*DWORD localCnt = InterlockedIncrement(&_logIdx);
				_LogArr[localCnt].ptr = newNode;
				_LogArr[localCnt].type = FREE;*/

				InterlockedIncrement(&_ulIDCnt);

				InterlockedIncrement(&_iLeftChunk);
				InterlockedDecrement(&_iUseChunk);
			}
		}
	}

	// TLS 스택에서 할당
	DATA* Alloc()
	{
		if (_TlsIdx == 0)
			DebugBreak();

		// 스레드의 메모리풀 주소 얻어오기
		TLSMemoryPool* pMemoryPool = (TLSMemoryPool*)TlsGetValue(_TlsIdx);
		if (pMemoryPool == NULL)
		{
			Thread_Init();

			pMemoryPool = (TLSMemoryPool*)TlsGetValue(_TlsIdx);
		}

		DATA* data = pMemoryPool->Alloc();

		if (_bPlacementNew)
		{
			data = new(data) DATA;
		}

		return data;
	}

	// 스레드 스택에 반환
	void Free(DATA* pData)
	{
		if (_TlsIdx == 0)
			DebugBreak();

		// 스레드의 메모리풀 주소 얻어오기
		TLSMemoryPool* pMemoryPool = (TLSMemoryPool*)TlsGetValue(_TlsIdx);
		if (pMemoryPool == NULL)
		{
			Thread_Init();

			pMemoryPool = (TLSMemoryPool*)TlsGetValue(_TlsIdx);
		}

		if (_bPlacementNew || _bCreateNew)
		{
			pData->~DATA();
		}

		pMemoryPool->Free(pData);
	}

	void CreateChunk()
	{
		st_BLOCK_NODE<DATA>* pNodeStart = (st_BLOCK_NODE<DATA>*)malloc(sizeof(st_BLOCK_NODE<DATA>) * _iChunkSize * _iCreateChunkCount);
		st_BLOCK_NODE<stChunk>* pChunkStart = (st_BLOCK_NODE<stChunk>*)malloc(sizeof(st_BLOCK_NODE<stChunk>) * _iCreateChunkCount);

		for (int chunkCount = 0; chunkCount < _iCreateChunkCount; chunkCount++)
		{
			st_BLOCK_NODE<DATA>* localTop = (st_BLOCK_NODE<DATA>*)pNodeStart;
			for (int i = 0; i < _iChunkSize; i++)
			{
				st_BLOCK_NODE<DATA>* pNode = pNodeStart + i;

				if (_bCreateNew)
				{
					new(&(pNode->allocData))DATA;
				}

				pNode->guardCode = _pGuardCode;
				pNode->nextPtr = localTop;
				memset(&pNode->allocData, 0, sizeof(DATA));

				localTop = pNode;
			}

			stChunk* ptr = &((st_BLOCK_NODE<stChunk>*)pChunkStart)->allocData;
			ptr->TopNode = localTop;
			ptr->BottomNode = pNodeStart;

			ULONGLONG localIdx = _ulIDCnt++;
			localIdx = localIdx << 47;

			pChunkStart->nextPtr = _TopNode;
			_TopNode = (st_BLOCK_NODE<stChunk>*)((ULONGLONG)pChunkStart | localIdx);

			pNodeStart = localTop + 1;
			pChunkStart += 1;
		}
	}

	// 얘가 스레드 별로 생성해서 TLS에 저장하는 클래스
	// 여러 스레드가 사용할 일이 없으니, 그냥 스택 구조로 구현
	class TLSMemoryPool
	{
	public:
		TLSMemoryPool(int baseChunk, int maxChunk, TLSMemoryPoolManager* manager)
		{
			_TopNode = NULL;
			_iTlsChunkSize = manager->_iChunkSize;

			_Size = 0;
			_iBaseChunk = baseChunk;
			_iBaseSize = baseChunk * _iTlsChunkSize;
			//_iMaxSize = maxChunk * _iChunkSize;

			// 애초에 처음 할당받는 basechunk + ALLOCCOUNT 보다 많아지면 반환할거야.
			//_pChunkArr = malloc(sizeof(stChunk) * )

			_Manager = manager;
		}

		// 메인 풀에서 청크를 할당받아 TLS 스택의 노드들과 연결해주고, 청크 배열에 저장
		void AllocChunkFromPool()
		{
			for (int i = 0; i < _iBaseChunk; i++)
			{
				stChunk* chunkPtr = _Manager->AllocChunkToTLS();

				((st_BLOCK_NODE<DATA>*)chunkPtr->BottomNode)->nextPtr = _TopNode;
				_TopNode = (st_BLOCK_NODE<DATA>*)chunkPtr->TopNode;

				_pChunkArr[_iTlsChunkCount++] = chunkPtr;

				_Size += _iTlsChunkSize;
			}
		}

		// 일단 하나씩 순회하며 카운팅해주고, 청크에서 꺼내서 반환하기. 그림은 그렸다.
		void FreeChunk()
		{
			// 뺄 청크보다 사이즈가 작으면 애초에 호출되면 안됐다.
			int chunkSize = _Manager->_iChunkSize;
			DWORD nowSize = _Size;
			if (_Size < chunkSize * ALLOCCOUNT)
				DebugBreak();

			LPVOID bottomNode;
			LPVOID topNode;
			for (int allocCnt = 0; allocCnt < ALLOCCOUNT; allocCnt++)
			{
				topNode = _TopNode;
				st_BLOCK_NODE<DATA>* pNode = _TopNode;
				for (int i = 0; i < _iTlsChunkSize; i++)
				{
					pNode = pNode->nextPtr;
				}
				bottomNode = pNode;

				_pChunkArr[_iTlsChunkCount] = 0;
				--_iTlsChunkCount;

				_pChunkArr[_iTlsChunkCount]->BottomNode = bottomNode;
				_pChunkArr[_iTlsChunkCount]->TopNode = topNode;

				_Manager->FreeChunkToPool(_pChunkArr[_iTlsChunkCount]);
				_Size -= _iTlsChunkSize;

				if (_Size < 0)
					DebugBreak();
			}
		}

		bool Free(DATA* pData)
		{
			st_BLOCK_NODE<DATA>* nodePtr = (st_BLOCK_NODE<DATA>*)((char*)pData - sizeof(void*));

#ifdef __GUARDTEST__
			if (nodePtr->guardCode != _pGuardCode || nodePtr->nextPtr != _pGuardCode)
			{
				DebugBreak();
			}
#endif

			nodePtr->nextPtr = _TopNode;
			_TopNode = nodePtr;

			//_workArr[_logIdx++] = { PUSH, nodePtr };

			++_Size;

			if (_Size > _iBaseSize + ALLOCCOUNT * _iTlsChunkSize)
			{
				FreeChunk();
			}

			return true;
		}

		DATA* Alloc()
		{
			// 그냥 부족할 때 할당해도 괜찮을듯?
			if (_TopNode == NULL)
				AllocChunkFromPool();

			st_BLOCK_NODE<DATA>* oldTop = _TopNode;

#ifdef __GUARDTEST__
			oldTop->nextPtr = (st_BLOCK_NODE*)m_guardCode;
#endif

			_TopNode = _TopNode->nextPtr;
			//_workArr[_logIdx++] = { POP, oldTop };

			--_Size;

			return &(oldTop->allocData);
		}

		/*bool Log(int num)
		{
			if (num < _logIdx)
			{
				switch (_workArr[num].first)
				{
				case PUSH:
					printf("PUSH : %p\n", _workArr[num].second);
					break;
				case POP:
					printf("POP : %p\n", _workArr[num].second);
					break;
				}
				return true;
			}
			return false;
		}*/

		stChunk _stChunk;
	private:
		DWORD _Size = 0;
		unsigned long _logIdx = 0;

		//pair<workType, void*> _workArr[LOGARR_MAX];

		unsigned int _iBaseChunk;
		unsigned int _iBaseSize;
		unsigned int _iMaxSize;

		unsigned int _iTlsChunkCount;
		unsigned int _iTlsChunkSize;
		stChunk* _pChunkArr[50];
		//stChunk* _pChunkArr;

		st_BLOCK_NODE<DATA>* _TopNode;

		TLSMemoryPoolManager* _Manager;
	};

	// 해당 스레드의 메모리풀이 몇번 TLS 인덱스에 박혀있는지 -> 각각 스레드의 _TlsIdx에 메모리풀 주소 저장
	DWORD _TlsIdx = -1;
private:
	//st_BLOCK_NODE* _TopNode;
	st_BLOCK_NODE<stChunk>* _TopNode;

	// 스레드 개수
	DWORD _ThreadCount;

	// 청크 사이즈, 생성 청크 개수
	unsigned int _iChunkSize;
	unsigned int _iCreateChunkCount;
	unsigned int _iThreadCount;
	unsigned int _iChunkPerThread;

	// 17비트 카운터
	ULONGLONG _ulIDCnt;

	// 현재 사용량, 남은 양, 용량
	unsigned int _iUseChunk;
	unsigned int _iLeftChunk;


	bool _bPlacementNew;
	bool _bCreateNew;
	void* _pGuardCode;
};


