#pragma once
#include <Windows.h>
// 메인 메모리 풀은 청크 단위로 오브젝트들을 관리하는 풀이다. 
// 락프리 구조로 구현.
#define ALLOCCOUNT 2
#define MAXCAPACITY_CHUNK 7

template <typename DATA>
class TLSMemoryPoolManager
{
	template <typename T>
	struct st_BLOCK_NODE
	{
		LPVOID guardCode;
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
		_ThreadCount = iThreadCount;

		_iChunkSize = iChunkSize;
		_iChunkPerThread = iChunkPerThread;

		// 총 생성 청크는 전체 스레드의 요구 청크 * 2만큼
		_iCreateChunkCount = iThreadCount * iChunkPerThread * 2;
		_iLeftChunk = _iCreateChunkCount;

		_bPlacementNew = bPlacementNew;
		_bCreateNew = bCreateNew;

		_TopUseChunk = NULL;

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
		pMemoryPool->AllocChunkFromPool();
	}

	// 종료할 때 동적할당 해제용도
	void Thread_CleanUp();

	// 청크 할당 메인 -> TLS
	st_BLOCK_NODE<DATA>* AllocChunkToTLS()
	{
		// 호출했으니까, 데이터를 반환할 때까지 루프
		while (1)
		{
			// 청크가 없다면 생성
			if (_TopUseChunk == NULL)
				CreateChunk();

			st_BLOCK_NODE<DATA>* oldTopChunk = _TopUseChunk;

			st_BLOCK_NODE<DATA>* chunkPtr = (st_BLOCK_NODE<DATA>*)(0x00007fffffffffff & (ULONGLONG)oldTopChunk);
			st_BLOCK_NODE<DATA>* newTopChunk = (st_BLOCK_NODE<DATA>*)chunkPtr->guardCode;

#ifdef __GUARDTEST__
			//chunkPtr->nextPtr = (st_BLOCK_NODE*)m_guardCode;
#endif

			if (InterlockedCompareExchange64((__int64*)&_TopUseChunk, (__int64)newTopChunk, (__int64)oldTopChunk) == (__int64)oldTopChunk)
			{
				/*DWORD localCnt = InterlockedIncrement(&_logIdx);
				_LogArr[localCnt].ptr = NodePtr;
				_LogArr[localCnt].type = ALLOC;*/

				InterlockedIncrement(&_iUseChunk);
				InterlockedDecrement(&_iLeftChunk);

				chunkPtr->guardCode = _pGuardCode;
				return oldTopChunk;
			}
		}
	}

	// 청크 해제 TLS -> 메인
	void FreeChunkToPool(st_BLOCK_NODE<DATA>* chunk)
	{
		//st_BLOCK_NODE<DATA>* chunkPtr = chunk//(st_BLOCK_NODE<DATA>*)((char*)chunk - sizeof(void*));
		st_BLOCK_NODE<DATA>* chunkPtr = (st_BLOCK_NODE<DATA>*)(0x00007fffffffffff & (ULONGLONG)chunk);
		ULONGLONG localIdx = InterlockedIncrement(&_ulIDCnt);
		localIdx = localIdx << 47;

#ifdef __GUARDTEST__
		if (chunkPtr->guardCode != _pGuardCode)
		{
			DebugBreak();
		}
#endif

		while (1)
		{
			st_BLOCK_NODE<DATA>* oldTopChunk = _TopUseChunk;
			chunkPtr->guardCode = (LPVOID)oldTopChunk;

			st_BLOCK_NODE<DATA>* newTop = (st_BLOCK_NODE<DATA>*)((ULONGLONG)chunkPtr | localIdx);

			if (InterlockedCompareExchange64((__int64*)&_TopUseChunk, (__int64)newTop, (__int64)oldTopChunk) == (__int64)oldTopChunk)
			{
				/*DWORD localCnt = InterlockedIncrement(&_logIdx);
				_LogArr[localCnt].ptr = newNode;
				_LogArr[localCnt].type = FREE;*/

				InterlockedIncrement(&_iLeftChunk);
				InterlockedDecrement(&_iUseChunk);
				break;
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

		if (_bPlacementNew)
		{
			pData->~DATA();
		}

		pMemoryPool->Free(pData);
	}

	void CreateChunk()
	{
		// TLS풀에 전달할 노드용 메모리
		st_BLOCK_NODE<DATA>* pNodeStart = (st_BLOCK_NODE<DATA>*)malloc(sizeof(st_BLOCK_NODE<DATA>) * _iChunkSize * _iCreateChunkCount);
		st_BLOCK_NODE<DATA>* prevNode = NULL;
		st_BLOCK_NODE<DATA>* pChunkNode = NULL;
		for (int chunkCount = 0; chunkCount < _iCreateChunkCount; chunkCount++)
		{
			for (int i = 0; i < _iChunkSize; i++)
			{
				pChunkNode = pNodeStart + i;

				if (_bCreateNew)
				{
					new(&(pChunkNode->allocData))DATA;
				}

				pChunkNode->guardCode = _pGuardCode;
				pChunkNode->nextPtr = prevNode;
				memset(&pChunkNode->allocData, 0, sizeof(DATA));

				prevNode = pChunkNode;
			}

			ULONGLONG localIdx = InterlockedIncrement(&_ulIDCnt);
			localIdx = localIdx << 47;

			while (1)
			{
				st_BLOCK_NODE<DATA>* oldChunkTop = _TopUseChunk;
				st_BLOCK_NODE<DATA>* newChunk = (st_BLOCK_NODE<DATA>*)((ULONGLONG)pChunkNode | localIdx);

				if (InterlockedCompareExchange64((__int64*)&_TopUseChunk, (__int64)newChunk, (__int64)oldChunkTop) == (__int64)oldChunkTop)
				{
					// 청크의 다음 노드 주소는 guardCode에 넣자. 어차피 청크 안에서의 guard처리는 안할거다.
					pChunkNode->guardCode = oldChunkTop;

					pNodeStart = pNodeStart + _iChunkSize;
					break;
				}
			}
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

			_dwSize = 0;
			_iBaseChunk = baseChunk;
			_iBaseSize = baseChunk * _iTlsChunkSize;
			//_iMaxSize = maxChunk * _iChunkSize;

			// 애초에 처음 할당받는 basechunk + ALLOCCOUNT 보다 많아지면 반환할거야.
			//_pChunkArr = malloc(sizeof(stChunk) * )

			_guardCode = manager;
			_Manager = manager;
		}

		// 메인 풀에서 청크를 할당받아 TLS 스택의 노드들과 연결해주고, 청크 배열에 저장
		void AllocChunkFromPool()
		{
			for (int i = 0; i < _iBaseChunk; i++)
			{
				// Chunk Data노드를 받는다.chunk만 인덱스를 사용하니 비트연산 필요
				st_BLOCK_NODE<DATA>* chunkTop = _Manager->AllocChunkToTLS();
				st_BLOCK_NODE<DATA>* chunkPtr = (st_BLOCK_NODE<DATA>*)(0x00007fffffffffff & (ULONGLONG)chunkTop);
				st_BLOCK_NODE<DATA>* bottomNode = chunkPtr;

				st_BLOCK_NODE<DATA>* arr[1000];
				// 그 노드를 타고 들어가서 최하단 노드를 찾기
				for (int j = 0; j < _iTlsChunkSize - 1; j++)
				{
					arr[j] = bottomNode;
					bottomNode = bottomNode->nextPtr;
				}
				// 여기서 오류나는데, 청크의 첫번째가 next가 NULL인게 문제가 되나? 확인해보니 첫 노드임.

				// bottomNode는 Top과 연결,Top은 청크로 받은 노드로 변경.
				bottomNode->nextPtr = _TopNode;
				_TopNode = chunkPtr;

				_dwSize += _iTlsChunkSize;
			}
		}

		// 일단 하나씩 순회하며 카운팅해주고, 청크에서 꺼내서 반환하기. 그림은 그렸다.
		void FreeChunk()
		{
			// 뺄 청크보다 사이즈가 작으면 애초에 호출되면 안됐다.
			DWORD nowSize = _dwSize;
			if (_dwSize < _iTlsChunkSize * ALLOCCOUNT)
				DebugBreak();

			// 반환할 청크 만큼 반복
			for (int allocCnt = 0; allocCnt < ALLOCCOUNT; allocCnt++)
			{
				// 현재 노드에서 Size만큼 탐색하며 그 다음 노드를 Top으로 설정
				st_BLOCK_NODE<DATA>* returnChunk = _TopNode;
				st_BLOCK_NODE<DATA>* newTopNode = _TopNode;
				for (int i = 0; i < _iTlsChunkSize; i++)
				{
					st_BLOCK_NODE<DATA>* newTopPtr = (st_BLOCK_NODE<DATA>*)(0x00007fffffffffff & (ULONGLONG)newTopNode);
					newTopNode = newTopPtr->nextPtr;
				}
				_TopNode = newTopNode;

				// 청크 데이터를 반환
				_Manager->FreeChunkToPool(returnChunk);
				_dwSize -= _iTlsChunkSize;
				if (_dwSize < 0)
					DebugBreak();
			}
		}

		bool Free(DATA* pData)
		{
			st_BLOCK_NODE<DATA>* nodePtr = (st_BLOCK_NODE<DATA>*)((char*)pData - sizeof(void*));

#ifdef __GUARDTEST__
			if (nodePtr->guardCode != _guardCode)
			{
				DebugBreak();
			}
#endif

			nodePtr->nextPtr = _TopNode;
			_TopNode = nodePtr;

			++_dwSize;

			if (_dwSize > _iBaseSize + ALLOCCOUNT * _iTlsChunkSize)
			{
				FreeChunk();
			}

			return true;
		}

		DATA* Alloc()
		{
			// 그냥 부족할 때 할당
			if (_TopNode == NULL)
				AllocChunkFromPool();

			st_BLOCK_NODE<DATA>* oldTop = _TopNode;

#ifdef __GUARDTEST__
			oldTop->guardCode = _pGuardCode;
#endif

			_TopNode = _TopNode->nextPtr;
			//_workArr[_logIdx++] = { POP, oldTop };

			--_dwSize;

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
	private:
		DWORD _dwSize = 0;
		unsigned long _logIdx = 0;

		//pair<workType, void*> _workArr[LOGARR_MAX];

		unsigned int _iBaseChunk;
		unsigned int _iBaseSize;
		unsigned int _iMaxSize;

		int _iTlsChunkCount;
		unsigned int _iTlsChunkSize;
		LPVOID _guardCode;

		st_BLOCK_NODE<DATA>* _TopNode;
		TLSMemoryPoolManager* _Manager;
	};

	// 해당 스레드의 메모리풀이 몇번 TLS 인덱스에 박혀있는지 -> 각각 스레드의 _TlsIdx에 메모리풀 주소 저장
	DWORD _TlsIdx = -1;
private:
	//st_BLOCK_NODE* _TopNode;
	st_BLOCK_NODE<DATA>* _TopUseChunk;

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


