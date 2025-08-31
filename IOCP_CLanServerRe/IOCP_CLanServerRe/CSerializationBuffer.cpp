#include "CSerializationBuffer.h"

procademy::CMemoryPool<CPacket> CPacket::_CPacketPool(0, true, false);

// 헤더를 넣을 직렬화버퍼는 이걸로 초기화
void CPacket::Initialize(int iBufferSize, int iHeaderSize)
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

CPacket::CPacket()
{
	_iBufferSize = eBUFFER_DFAULT;
	_head = 0;
	_tail = 0;
	_iDataSize = 0;
	_iBuffer = (char*)malloc(_iBufferSize);
	if (_iBuffer == nullptr)
	{
		DebugBreak();
		return;
	}
}

CPacket::CPacket(int iBufferSize)
{
	_iBufferSize = iBufferSize;
	_head = 0;
	_tail = 0;
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

CPacket::~CPacket()
{
	free(_iBuffer);
}

void CPacket::PushHeader(char* header, int headerSize)
{
	_head -= headerSize;

	memcpy(_iBuffer + _head, header, headerSize);

	_iDataSize += headerSize;
}