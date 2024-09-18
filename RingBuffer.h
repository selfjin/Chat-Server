#pragma once
#include <iostream>
#include <Windows.h>


//-------------------------------------------------------------
// define 및 전역 변수
//-------------------------------------------------------------

#define RINBBUFFER_SIZE 10000


// 해당 링 버퍼는 사이즈를 반드시 체크해서, 넣을 수 있을 때 하세요.
// 만약 사이즈가 Free Size 만큼을 넘으면, 짤라내고 넣습니다. 진짜. 
// 무 조 건 Capacity - Size = Free Size 체크

class RingBuffer
{
public:
    RingBuffer(int capacity);
    ~RingBuffer();

    int getSize() { return _size; }
    int getCapacity() const { return _capacity; }
    int Enqueue(const char* data, int bytes);
    int Dequeue(char* data, int bytes);
    int peek(char* data, int bytes);


    ///////////////////////////////////////////////


    char* GetFrontBufferPtr() { return &buffer[_begin_index]; }
    char* getRearBufferPtr() { return &buffer[_end_index]; }

    void moveBegin(int value);

    ///////////////////////////////////////////////

private:
    int _begin_index;
    int _end_index;
    int _size;
    int _capacity;
    char* buffer;
};

