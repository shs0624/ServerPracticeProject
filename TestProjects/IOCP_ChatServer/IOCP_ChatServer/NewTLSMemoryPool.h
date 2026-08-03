#pragma once
#include <Windows.h>
#include "ProcademyProfiler.h"
// 메인 메모리 풀은 청크 단위로 오브젝트들을 관리하는 풀이다. 
// 메인 - 락프리 스택
// TLS 메모리풀 - 배열에서 차례대로 쓰는 구조
// TLS 풀에서 사용이 끝나면 청크를 교체하고, 메인 풀에서 하나 가져와서 대기시킴.
// 교체해서 나간 청크는 밖에 돌아다니며, 모든 노드가 돌아오길 기다림.
// r
#define ALLOCCOUNT 2
#define MAXCAPACITY_CHUNK 7
#define LOGSIZE 10000
//#define DEBUG_TLSMEMORYPOOL

enum LOG_WORKTYPE
{
	ALLOC_TLSPOOL,
	FREE_TLSPOOL
};

template <typename DATA>
class NewTLSMemoryPoolManager
{
	struct st_BLOCK_NODE
	{
		LPVOID guardCode;
		DATA allocData;
		// 이거 앞 17비트에 인덱스를 넣자.
		LPVOID pReturnChunk;
	};

	struct st_Chunk
	{
		NewTLSMemoryPoolManager* _pManager;
		LPVOID _pNextChunk;

		st_BLOCK_NODE* _pNodeArr;
		bool* _pCheckArr;
		// Alloc은 idx를 보며 하나 하나 해주는 방식. 남은 노드가 없다면 교체
		// Free는 이 배열로 돌아오고, 비트 플래그를 세워서 교체하는 방식.
		DWORD _dwUseNodeCount;
		DWORD _dwReturnNodeCount;
	};

	struct st_ALLOCLOG
	{
		LOG_WORKTYPE type;
		st_BLOCK_NODE* ptr;
	};

	//friend class TLSMemoryPool;
public:
	// 매개변수 (1청크에 들어가는 노드 개수, 스레드에 할당할 기본 청크 개수, 스레드 개수, 할당 받을때 생성자 호출 여부, 생성 때 생성자 호출 여부)
	NewTLSMemoryPoolManager(unsigned int iChunkSize = 0, unsigned int iThreadCount = 0,
		bool bPlacementNew = false, bool bCreateNew = false)
	{
		_TlsIdx = TlsAlloc();
		_ThreadCount = iThreadCount;

		_iChunkSize = iChunkSize;

		// 총 생성 청크는 전체 스레드의 * 4만큼
		_iCreateChunkCount = iThreadCount * 4;

		_bPlacementNew = bPlacementNew;
		_bCreateNew = bCreateNew;

		_TopChunk = NULL;

		_pGuardCode = (LPVOID)this;

		InitChunk();
	}

	void InitChunk()
	{
		for (int chunkCount = 0; chunkCount < _iCreateChunkCount; chunkCount++)
		{
			CreateChunk();
		}
	}

	// 스레드 별 스택 생성
	void Thread_Init()
	{
#ifdef DEBUG_TLSMEMORYPOOL
		if (_TlsIdx == 0)
			DebugBreak();
#endif

		// 스레드의 메모리풀 주소 얻어오기
		TLSMemoryPool* pMemoryPool = new TLSMemoryPool(this);
		TlsSetValue(_TlsIdx, (LPVOID)pMemoryPool);

		// 여기에 내가 생성해놓은 노드들 단체로 이동
		pMemoryPool->InitTLSMemoryPool();

		InterlockedIncrement(&_iTLSPoolCount);
	}

	// 종료할 때 동적할당 해제용도
	void Thread_CleanUp();

	// 청크 할당 메인 -> TLS
	st_Chunk* AllocChunkToTLS()
	{
		// 호출했으니까, 데이터를 반환할 때까지 루프
		while (1)
		{
			// 청크가 없다면 생성
			if (_TopChunk == NULL)
			{
				CreateChunk();
			}

			st_Chunk* oldTopChunk = _TopChunk;

			st_Chunk* chunkPtr = (st_Chunk*)(0x00007fffffffffff & (ULONGLONG)oldTopChunk);
			st_Chunk* newTopChunk = (st_Chunk*)chunkPtr->_pNextChunk;

			if (InterlockedCompareExchange64((__int64*)&_TopChunk, (__int64)newTopChunk, (__int64)oldTopChunk) == (__int64)oldTopChunk)
			{
#ifdef DEBUG_TLSMEMORYPOOL
				DWORD localCnt = InterlockedIncrement(&_logIdx) % LOGSIZE;
				_LogArr[localCnt].ptr = oldTopChunk;
				_LogArr[localCnt].type = ALLOC_TLSPOOL;

				chunkPtr->guardCode = _pGuardCode;
#endif

				InterlockedIncrement(&_iUseChunk);
				InterlockedDecrement(&_iLeftChunk);

				//chunkPtr->guardCode = _pGuardCode;
				return chunkPtr;
			}
		}
	}

	// 청크 해제 TLS -> 메인
	void FreeChunkToPool(st_Chunk* chunk)
	{
		ULONGLONG localIdx = InterlockedIncrement(&_ulIDCnt);
		localIdx = localIdx << 47;

		while (1)
		{
			st_Chunk* oldTopChunk = _TopChunk;
			st_Chunk* newTop = (st_Chunk*)((ULONGLONG)chunk | localIdx);

			if (InterlockedCompareExchange64((__int64*)&_TopChunk, (__int64)newTop, (__int64)oldTopChunk) == (__int64)oldTopChunk)
			{
				chunk->_pNextChunk = oldTopChunk;

#ifdef DEBUG_TLSMEMORYPOOL
				DWORD localCnt = InterlockedIncrement(&_logIdx) % LOGSIZE;
				_LogArr[localCnt].ptr = newTop;
				_LogArr[localCnt].type = FREE_TLSPOOL;
#endif

				InterlockedIncrement(&_iLeftChunk);
				InterlockedDecrement(&_iUseChunk);
				break;
			}
		}
	}

	// TLS 스택에서 할당
	DATA* Alloc()
	{
#ifdef DEBUG_TLSMEMORYPOOL
		if (_TlsIdx == 0)
			DebugBreak();
#endif

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

		InterlockedIncrement(&_dwAllocCount);

		return data;
	}

	// 스레드 스택에 반환
	void Free(DATA* pData)
	{
#ifdef DEBUG_TLSMEMORYPOOL
		if (_TlsIdx == 0)
			DebugBreak();
#endif

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

		InterlockedIncrement(&_dwFreeCount);
	}

	void CreateChunk()
	{
		st_Chunk* pChunk = (st_Chunk*)malloc(sizeof(st_Chunk));
		st_BLOCK_NODE* pNodeArr = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE) * _iChunkSize);

		pChunk->_pManager = this;
		pChunk->_pNodeArr = pNodeArr;

		for (int i = 0; i < _iChunkSize; i++)
		{
			pNodeArr[i].pReturnChunk = (LPVOID)pChunk;
		}
		pChunk->_dwReturnNodeCount = 0;
		pChunk->_dwUseNodeCount = 0;

		ULONGLONG localIdx = InterlockedIncrement(&_ulIDCnt);
		localIdx = localIdx << 47;

		while (1)
		{
			st_Chunk* oldChunkTop = _TopChunk;
			st_Chunk* newChunk = (st_Chunk*)((ULONGLONG)pChunk | localIdx);

			if (InterlockedCompareExchange64((__int64*)&_TopChunk, (__int64)newChunk, (__int64)oldChunkTop) == (__int64)oldChunkTop)
			{
				pChunk->_pNextChunk = oldChunkTop;
				InterlockedIncrement(&_iLeftChunk);
				break;
			}
		}
	}

	// 얘가 스레드 별로 생성해서 TLS에 저장하는 클래스
	// 여러 스레드가 사용할 일이 없으니, 그냥 스택 구조로 구현
	class TLSMemoryPool
	{
	public:
		TLSMemoryPool(NewTLSMemoryPoolManager* manager)
		{
			_TopNode = NULL;
			_iTlsChunkSize = manager->_iChunkSize;

			_dwSize = 0;
			_iBaseSize = _iTlsChunkSize;
			//_iMaxSize = maxChunk * _iChunkSize;

			_guardCode = manager;
			_Manager = manager;
		}

		void InitTLSMemoryPool()
		{
			_pUseChunk = _Manager->AllocChunkToTLS();
			_pSpareChunk = _Manager->AllocChunkToTLS();
		}

		bool Free(DATA* pData)
		{
			//st_BLOCK_NODE<DATA>* nodePtr = (st_BLOCK_NODE<DATA>*)((char*)pData - sizeof(LPVOID));
			st_BLOCK_NODE* nodePtr = (st_BLOCK_NODE*)((char*)pData - offsetof(st_BLOCK_NODE, allocData));
			st_Chunk* chunkPtr = (st_Chunk*)nodePtr->pReturnChunk;

//			DWORD localCnt = InterlockedIncrement(&_dwTLSLogIdx) % LOGSIZE;
//			_TLSLogArr[localCnt].ptr = nodePtr;
//			_TLSLogArr[localCnt].type = FREE_TLSPOOL;

			InterlockedIncrement(&chunkPtr->_dwReturnNodeCount);

			// 이미 사용이 끝난 청크라면, 청크의 매니저 반환 여부 체크
			if (chunkPtr->_dwReturnNodeCount == _iTlsChunkSize && chunkPtr->_dwUseNodeCount == _iTlsChunkSize)
			{
				chunkPtr->_pManager->FreeChunkToPool(chunkPtr);
			}
//
			return true;
		}

		DATA* Alloc()
		{
			// @@TODO : 현재 사용중인 청크가 비었는지 확인
			if (_iTlsChunkSize - _pUseChunk->_dwUseNodeCount <= 0)
			{
				DebugBreak();
			}

//			DWORD localCnt = InterlockedIncrement(&_dwTLSLogIdx) % LOGSIZE;
//			_TLSLogArr[localCnt].ptr = oldTop;
//			_TLSLogArr[localCnt].type = ALLOC_TLSPOOL;

			st_BLOCK_NODE* allocNode = &(_pUseChunk->_pNodeArr[_pUseChunk->_dwUseNodeCount]);
			InterlockedIncrement(&_pUseChunk->_dwUseNodeCount);
						
			if (_pUseChunk->_dwUseNodeCount == _iTlsChunkSize)
			{
				// 사용 종료. 노드 전부 사용됨
				Swap();
			}

			return &(allocNode->allocData);
		}

		// 예비 청크의 Top노드로 현재 Top노드를 교체.
		void Swap()
		{
			_pUseChunk = _pSpareChunk;
			_pUseChunk->_dwUseNodeCount = 0;
			_pUseChunk->_dwReturnNodeCount = 0;

			_pSpareChunk = _Manager->AllocChunkToTLS();
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
		st_Chunk* _pUseChunk;
		st_Chunk* _pSpareChunk;

		/// <summary>
		/// ////////////////////////////////////////////////////////////////////
		/// </summary>

		DWORD _dwTLSLogIdx;
		st_ALLOCLOG _TLSLogArr[LOGSIZE];
		
		DWORD _dwSize = 0;
		unsigned long _logIdx = 0;

		unsigned int _iBaseSize;
		unsigned int _iMaxSize;

		int _iTlsChunkCount;
		int _iTlsChunkSize;
		LPVOID _guardCode;

		st_BLOCK_NODE* _TopNode;
		NewTLSMemoryPoolManager* _Manager;
	};

	// 해당 스레드의 메모리풀이 몇번 TLS 인덱스에 박혀있는지 -> 각각 스레드의 _TlsIdx에 메모리풀 주소 저장
	DWORD _TlsIdx = -1;
private:
	st_Chunk* _TopChunk;

	// 스레드 개수
	DWORD _ThreadCount;

	// 청크 사이즈, 생성 청크 개수
	unsigned int _iChunkSize;
	unsigned int _iCreateChunkCount;
	unsigned int _iThreadCount;

	// 17비트 카운터
	ULONGLONG _ulIDCnt;

	unsigned int _iTLSPoolCount;

	// 현재 사용량, 남은 양, 용량
	unsigned int _iUseChunk;
	unsigned int _iLeftChunk;

	DWORD _logIdx;
	st_ALLOCLOG _LogArr[LOGSIZE];

	DWORD _dwFreeCount;
	DWORD _dwAllocCount;

	bool _bPlacementNew;
	bool _bCreateNew;
	void* _pGuardCode;
};


