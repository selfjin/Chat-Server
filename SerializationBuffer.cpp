#include "SerializationBuffer.h"


CPacket::CPacket() :
    _begin_index(0),
    _end_index(0),
    _size(0),
    _capacity(en_PACKET::eBUFFER_DEFAULT)
{
    buffer = new char[_capacity];
}

CPacket::CPacket(int capacity) :
    _begin_index(0),
    _end_index(0),
    _size(0),
    _capacity(capacity)
{
    buffer = new char[_capacity];
}

CPacket::~CPacket()
{
    delete[] buffer;
}

int CPacket::Enqueue(char* data, int bytes)
{

#ifdef DEBUG
    if (bytes > _capacity - _size)
    {
        DebugBreak;
    }
    memcpy(buffer + _end_index, data, bytes);
    _end_index += bytes;
    _size += bytes;
#endif

    memcpy(buffer + _end_index, data, bytes);
    _end_index += bytes;
    _size += bytes;

    return bytes;

}

int CPacket::Dequeue(char* data, int bytes)
{

#ifdef DEBUG
    if (bytes > _size)
    {
        DebugBreak;
    }
#endif

    memcpy(data, buffer + _begin_index, bytes);
    _begin_index += bytes;
    _size -= bytes;

    return bytes;
}

void CPacket::Clear(void)
{
    _capacity = eBUFFER_DEFAULT;
    _begin_index = _end_index = 0;
    _size = 0;
}

int CPacket::moveBegin(int value)
{
    _begin_index += value;

#ifdef DEBUG
    if (_begin_index > _end_index)
    {
        DebugBreak;
    }
#endif

    _size -= value;

    return _begin_index;
}

// _end_index --> Right Move
int CPacket::moveEnd(int value)
{
    _end_index += value;

#ifdef DEBUG
    if (_end_index > _capacity)
    {
        DebugBreak;
    }
#endif

    _size += value;

    return _end_index;
}

// 대입 연산자 오버로딩
CPacket& CPacket::operator<<(unsigned char byValue)
{
    *(reinterpret_cast<unsigned char*>(buffer + _end_index)) = byValue;
    _end_index += sizeof(byValue);
    _size += sizeof(byValue);
    return *this;
}

CPacket& CPacket::operator<<(char chValue)
{
    *(reinterpret_cast<char*>(buffer + _end_index)) = chValue;
    _end_index += sizeof(chValue);
    _size += sizeof(chValue);
    return *this;
}

CPacket& CPacket::operator << (WCHAR chValue)
{
    *(reinterpret_cast<WCHAR*>(buffer + _end_index)) = chValue;
    _end_index += sizeof(chValue);
    _size += sizeof(chValue);
    return *this;
}


CPacket& CPacket::operator<<(short shValue)
{
    *(reinterpret_cast<short*>(buffer + _end_index)) = shValue;
    _end_index += sizeof(shValue);
    _size += sizeof(shValue);
    return *this;
}

CPacket& CPacket::operator<<(unsigned short wValue)
{
    *(reinterpret_cast<unsigned short*>(buffer + _end_index)) = wValue;
    _end_index += sizeof(wValue);
    _size += sizeof(wValue);
    return *this;
}

CPacket& CPacket::operator<<(int iValue)
{
    *(reinterpret_cast<int*>(buffer + _end_index)) = iValue;
    _end_index += sizeof(iValue);
    _size += sizeof(iValue);
    return *this;
}

CPacket& CPacket::operator<<(long lValue)
{
    *(reinterpret_cast<long*>(buffer + _end_index)) = lValue;
    _end_index += sizeof(lValue);
    _size += sizeof(lValue);
    return *this;
}

CPacket& CPacket::operator<<(float fValue)
{
    *(reinterpret_cast<float*>(buffer + _end_index)) = fValue;
    _end_index += sizeof(fValue);
    _size += sizeof(fValue);
    return *this;
}

CPacket& CPacket::operator<<(__int64 iValue)
{
    *(reinterpret_cast<__int64*>(buffer + _end_index)) = iValue;
    _end_index += sizeof(iValue);
    _size += sizeof(iValue);
    return *this;
}

CPacket& CPacket::operator<<(double dValue)
{
    *(reinterpret_cast<double*>(buffer + _end_index)) = dValue;
    _end_index += sizeof(dValue);
    _size += sizeof(dValue);
    return *this;
}

// >> 연산자 오버로딩 (데이터 추출)
CPacket& CPacket::operator>>(BYTE& byValue)
{
    byValue = *(reinterpret_cast<BYTE*>(buffer + _begin_index));
    _begin_index += sizeof(byValue);
    _size -= sizeof(byValue);
    return *this;
}

CPacket& CPacket::operator>>(char& chValue)
{
    chValue = *(reinterpret_cast<char*>(buffer + _begin_index));
    _begin_index += sizeof(chValue);
    _size -= sizeof(chValue);
    return *this;
}

CPacket& CPacket::operator>>(WCHAR& chValue)
{
    chValue = *(reinterpret_cast<WCHAR*>(buffer + _begin_index));
    _begin_index += sizeof(chValue);
    _size -= sizeof(chValue);
    return *this;
}


CPacket& CPacket::operator>>(short& shValue)
{
    shValue = *(reinterpret_cast<short*>(buffer + _begin_index));
    _begin_index += sizeof(shValue);
    _size -= sizeof(shValue);
    return *this;
}

CPacket& CPacket::operator>>(WORD& wValue)
{
    wValue = *(reinterpret_cast<WORD*>(buffer + _begin_index));
    _begin_index += sizeof(wValue);
    _size -= sizeof(wValue);
    return *this;
}

CPacket& CPacket::operator>>(int& iValue)
{
    iValue = *(reinterpret_cast<int*>(buffer + _begin_index));
    _begin_index += sizeof(iValue);
    _size -= sizeof(iValue);
    return *this;
}

CPacket& CPacket::operator>>(DWORD& dwValue)
{
    dwValue = *(reinterpret_cast<DWORD*>(buffer + _begin_index));
    _begin_index += sizeof(dwValue);
    _size -= sizeof(dwValue);
    return *this;
}

CPacket& CPacket::operator>>(float& fValue)
{
    fValue = *(reinterpret_cast<float*>(buffer + _begin_index));
    _begin_index += sizeof(fValue);
    _size -= sizeof(fValue);
    return *this;
}

CPacket& CPacket::operator>>(__int64& iValue)
{
    iValue = *(reinterpret_cast<__int64*>(buffer + _begin_index));
    _begin_index += sizeof(iValue);
    _size -= sizeof(iValue);
    return *this;
}

CPacket& CPacket::operator>>(double& dValue)
{
    dValue = *(reinterpret_cast<double*>(buffer + _begin_index));
    _begin_index += sizeof(dValue);
    _size -= sizeof(dValue);
    return *this;
}

