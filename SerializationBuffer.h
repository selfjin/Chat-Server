#pragma once

#include <Windows.h>

class CPacket
{
public:

	/*---------------------------------------------------------------
	Packet Enum.

	----------------------------------------------------------------*/
	enum en_PACKET
	{
		eBUFFER_DEFAULT = 1000		// 패킷의 기본 버퍼 사이즈.
	};

	CPacket();
	CPacket(int capacity);
	virtual	~CPacket();

	int getCapacity() { return _capacity; }
	int getSize() { return _size; }
	int Enqueue(char* data, int bytes);
	int Dequeue(char* data, int bytes);

	void Clear(void);



	///////////////////////////////////////////////
	int		moveBegin(int value);
	int		moveEnd(int value);

	char* GetBufferPtr(void) { return buffer; }		// Buffer의 주소를 반환

	///////////////////////////////////////////////

	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	CPacket& operator = (CPacket& clSrcPacket);

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CPacket& operator << (unsigned char byValue);
	CPacket& operator << (char chValue);
	CPacket& operator << (WCHAR& chValue);

	CPacket& operator << (short shValue);
	CPacket& operator << (unsigned short wValue);

	CPacket& operator << (int iValue);
	CPacket& operator << (long lValue);
	CPacket& operator << (float fValue);

	CPacket& operator << (__int64 iValue);
	CPacket& operator << (double dValue);


	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CPacket& operator >> (BYTE& byValue);
	CPacket& operator >> (char& chValue);
	

	CPacket& operator >> (short& shValue);
	CPacket& operator >> (WORD& wValue);

	CPacket& operator >> (int& iValue);
	CPacket& operator >> (DWORD& dwValue);
	CPacket& operator >> (float& fValue);

	CPacket& operator >> (__int64& iValue);
	CPacket& operator >> (double& dValue);



protected:
	int _begin_index;
	int _end_index;
	int	_capacity;	// 가용 최대 사이즈
	int	_size;		// 사용 중인 사이즈
	char* buffer;
};