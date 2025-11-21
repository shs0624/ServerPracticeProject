#include "CSerializationBuffer.h"

//procademy::CMemoryPool_LockFree<CPacket> CPacket::_CPacketPool(0, true, false);
//TLSMemoryPoolManager<CPacket> CPacket::_CPacketPool(100, 5, 5, true, false);
procademy::MemoryPool_TLS<CPacket> CPacket::_CPacketPool(500, true);
DWORD CPacket::_iLogFreeIdx;
LPVOID CPacket::_freeLog[CPACKET_LOGSIZE];
DWORD CPacket::_iLogAllocIdx;
LPVOID CPacket::_allocLog[CPACKET_LOGSIZE];

// 직렬화버퍼 초기화. 호출 필수적. 헤더를 넣었다면 헤더 사이즈도 설정
void CPacket::Initialize(int iBufferSize, int iHeaderSize = 0)
{
	_iBufferSize = iBufferSize;
	_head = iHeaderSize;
	_tail = iHeaderSize;
	_iDataSize = 0;
	_iHeaderSize = iHeaderSize;
	_iBuffer = (char*)malloc(_iBufferSize);
	if (_iBuffer == nullptr)
	{
		DebugBreak();
		return;
	}

	if (_isUsing == TRUE)
		DebugBreak();
	_isUsing = TRUE;

	int idx = InterlockedIncrement(&_iLogAllocIdx) % CPACKET_LOGSIZE;
	_allocLog[idx] = (LPVOID)this;
}

#pragma warning(disable:26495)
CPacket::CPacket()
{

}
#pragma warning(default:26495)

void CPacket::SetCheckSum()
{
	unsigned char* payloadPtr = (unsigned char*)_iBuffer + _iHeaderSize;
	unsigned char* tailPtr = (unsigned char*)_iBuffer + _tail;

	unsigned long sum = 0;
	while (payloadPtr != tailPtr)
	{
		//sum += (unsigned char)*(_iBuffer + _iHeaderSize) + 1;
		sum += (unsigned char)*payloadPtr;
		payloadPtr++;
	}

	unsigned char checkSum = (unsigned char)(sum % 256);
	*(GetCheckSumPtr()) = checkSum;
}

unsigned char CPacket::GetCheckSum()
{
	unsigned char* payloadPtr = (unsigned char*)_iBuffer + _iHeaderSize;
	unsigned char* tailPtr = (unsigned char*)_iBuffer + _tail;

	unsigned long sum = 0;
	while (payloadPtr != tailPtr)
	{
		sum += (unsigned char)*payloadPtr;
		payloadPtr++;
	}

	unsigned char checkSum = (unsigned char)(sum % 256);
	return checkSum;
}

void CPacket::Encode(unsigned char K)
{
	unsigned char RK = rand();
	SetCheckSum();

	unsigned char* cursorPtr = (unsigned char*)GetCheckSumPtr();
	unsigned char* tailPtr = (unsigned char*)_iBuffer + _tail;

	unsigned char E = 0;
	unsigned char P = 0;

	int cnt = 1;
	while (cursorPtr != tailPtr)
	{
		unsigned char D = *cursorPtr;

		P = D ^ (P + RK + cnt);
		E = P ^ (E + K + cnt);

		*cursorPtr = E;

		cursorPtr++;
		cnt++;
	}
}

bool CPacket::Decode(unsigned char K, unsigned char RK)
{
	unsigned char* cursorPtr = (unsigned char*)GetCheckSumPtr();
	unsigned char* tailPtr = (unsigned char*)_iBuffer + _tail;

	unsigned char D = 0;
	unsigned char P = 0;
	unsigned char prevE = 0;
	unsigned char prevP = 0;

	int cnt = 1;
	while (cursorPtr != tailPtr)
	{
		unsigned char E = *cursorPtr;

		P = E ^ (prevE + K + cnt);
		D = P ^ (prevP + RK + cnt);

		prevP = P;
		prevE = E;

		*cursorPtr = D;
		cursorPtr++;
		cnt++;
	}

	unsigned char checkSum = GetCheckSum();
	if (checkSum != *GetCheckSumPtr())
		return false;

	return true;
}


int CPacket::GetData(char* chpDest, int iSize)
{
	int getSize = (iSize > _iDataSize) ? _iDataSize : iSize;
	memcpy(chpDest, _iBuffer + _head, getSize);

	_iDataSize -= getSize;
	_head += getSize;
	return getSize;
}

int CPacket::PutData(char* chpDest, int iSize)
{
	int putSize = (_tail + iSize > _iBufferSize) ? ((_tail + iSize) - _iBufferSize) : iSize;
	memcpy(_iBuffer + _tail, chpDest, putSize);

	_iDataSize += putSize;
	_tail += putSize;
	return putSize;
}

int	CPacket::MoveWritePos(int iSize)
{
	int moveSize = (_tail + iSize > _iBufferSize) ? ((_tail + iSize) - _iBufferSize) : iSize;
	_iDataSize += moveSize;
	_tail += moveSize;
	return moveSize;
}

int	CPacket::MoveReadPos(int iSize)
{
	int moveSize = (iSize > _iDataSize) ? _iDataSize : iSize;
	_head += moveSize;
	_iDataSize -= moveSize;
	return moveSize;
}

void CPacket::Clear(void)
{
	_head = 0;
	_tail = 0;
	_iDataSize = 0;
}

CPacket::~CPacket()
{
	int idx = InterlockedIncrement(&_iLogFreeIdx) % CPACKET_LOGSIZE;
	_freeLog[idx] = (LPVOID)this;

	free(_iBuffer);
	_iBuffer = nullptr;

	if (_isUsing == FALSE)
		DebugBreak();
	_isUsing = FALSE;
}

void CPacket::PushHeader(char* header, int headerSize)
{
	_head -= headerSize;

	memcpy(_iBuffer + _head, header, headerSize);

	_iDataSize += headerSize;
}