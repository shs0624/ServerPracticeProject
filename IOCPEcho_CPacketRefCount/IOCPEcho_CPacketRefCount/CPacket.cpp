#include "CPacket.h"
#include <iostream>

CPacket::CPacket()
{
	
}

CPacket::~CPacket()
{
	free(_iBuffer);
}

void CPacket::Initialize(int iBufferSize = eBUFFER_DFAULT, int iHeaderSize = 0)
{
	_iBufferSize = iBufferSize;
	_head = iHeaderSize;
	_tail = iHeaderSize;
	_iDataSize = 0;
	_iBuffer = (char*)malloc(_iBufferSize);
	if (_iBuffer == nullptr)
	{
		DebugBreak();
		return;
	}
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

//------------------------------------------------------------------
// 내 라이브러리 전용 함수

// head 앞에 헤더 넣는 함수
void CPacket::PushHeader(char* header, int headerSize)
{
	_head -= headerSize;

	memcpy(_iBuffer + _head, header, headerSize);

	_iDataSize += headerSize;
}