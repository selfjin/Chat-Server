#pragma once
#include <stddef.h>
#include <Windows.h>

//////////////////////////////////////////////////
// 1. DEBUG 빌드일 때, 메모리 범퍼(GUARD) 추가
#define GUARD1 0xFFFFFFFF
#define GUARD2 0xFFFFFFFF
//////////////////////////////////////////////////

#ifdef _DEBUG

#pragma pack(1)
template <typename T>
struct ObjectPoolNode
{
#ifdef _DEBUG
    int guard1 = GUARD1;
    T _data;
    int guard2 = GUARD2;
    ObjectPoolNode* next;
#else
    T _data;
    ObjectPoolNode* next;
#endif
};
#pragma pack(pop)
#else
template <typename T>
struct ObjectPoolNode
{
#ifdef _DEBUG
    int guard1 = GUARD1;
    T _data;
    int guard2 = GUARD2;
    ObjectPoolNode* next;
#else
    T _data;
    ObjectPoolNode* next;
#endif
};
#endif


template <typename T>
class ObjectMemoryPool
{
public:
    ObjectMemoryPool(int capacity = 10000);
    ~ObjectMemoryPool();

    // 메모리만 할당 (생성자는 호출 안함)
    T* Alloc();

    // 메모리 할당과 동시에 생성자 호출
    T* Alloc_Constructor();

    // 메모리 반환 (소멸자는 호출 안함)
    bool Free(T* object);

    // 메모리 반환 (소멸자 호출)
    bool Free_Destructor(T* object);

    int getSize() { return _size; }

private:
    ObjectPoolNode<T>* _top;
    int _size;
    int _availableSize;
};

//////////////////////////////////////////////////
// 템플릿 클래스의 구현부
//////////////////////////////////////////////////

template<typename T>
inline ObjectMemoryPool<T>::ObjectMemoryPool(int initCapacity) : _top(nullptr), _size(0), _availableSize(0)
{

#ifdef _DEBUG
    // 초기 capacity만큼의 노드를 미리 할당
    for (int i = 0; i < initCapacity; i++)
    {
        ObjectPoolNode<T>* newNode = new ObjectPoolNode<T>;
        newNode->next = _top;
        _top = newNode;
        _size++;
        _availableSize++;
    }
#else
    for (int i = 0; i < initCapacity; i++)
    {
        ObjectPoolNode<T>* newNode = new ObjectPoolNode<T>;
        newNode->next = _top;
        _top = newNode;
    }
#endif
}

template<typename T>
ObjectMemoryPool<T>::~ObjectMemoryPool()
{
    // 풀에 남아 있는 모든 노드들을 해제
    while (_top)
    {
        ObjectPoolNode<T>* node = _top;
        _top = _top->next;
        delete node;
    }
}

template<typename T>
inline T* ObjectMemoryPool<T>::Alloc()
{
#ifdef _DEBUG
    if (_top->next == nullptr)
    {
        // 사용 가능한 노드가 없으면 새로 생성
        ObjectPoolNode<T>* newNode = new ObjectPoolNode<T>;
        newNode->next = nullptr;
        _top = newNode;
        _size++;
        return &(newNode->_data);
    }
    else
    {
        // 사용 가능한 노드가 있으면 반환
        T* returnPtr = &(_top->_data);
        _top = _top->next;
        _availableSize--;
        return returnPtr;
    }
#else
    if (_top->next == nullptr)
    {
        // 사용 가능한 노드가 없으면 새로 생성
        ObjectPoolNode<T>* newNode = (ObjectPoolNode<T>*)malloc(sizeof(ObjectPoolNode<T>));
        newNode->next = nullptr;
        _top = newNode;
        return &(newNode->_data);
    }
    else
    {
        // 사용 가능한 노드가 있으면 반환
        T* returnPtr = &(_top->_data);
        _top = _top->next;
        return returnPtr;
    }
#endif

}



//////////////////////////////////////////// 미완성 ////////////////////////////////////////////
template<typename T>
inline T* ObjectMemoryPool<T>::Alloc_Constructor()
{
    if (_availableSize == 0)
    {
        // 사용 가능한 노드가 없으면 새로 생성
        ObjectPoolNode<T>* newNode = new ObjectPoolNode<T>;
        newNode->next = nullptr;
        _top = newNode;
        _size++;
        return &(newNode->_data);
    }
    else
    {
        // 노드를 재활용하면서, 생성자를 호출
        T* returnPtr = new (&(_top->_data)) T; // placement new
        _top = _top->next;
        _availableSize--;
        return returnPtr;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////


template<typename T>
inline bool ObjectMemoryPool<T>::Free(T* object)
{
#ifdef _DEBUG
    // object는 _data를 가리키고 있으므로 ObjectPoolNode<T>로 변환
    ObjectPoolNode<T>* node = reinterpret_cast<ObjectPoolNode<T>*>(reinterpret_cast<char*>(object) - offsetof(ObjectPoolNode<T>, _data));

    // 가드 값 검사
    if (node->guard1 != GUARD1 || node->guard2 != GUARD2)
    {
        DebugBreak(); // 가드 값이 맞지 않으면 디버그 브레이크
        return false;
    }
    node->next = _top;
    _top = node;
    return true;
#else
    // object의 메모리를 풀이 가리키는 _top으로 돌려놓음
    ObjectPoolNode<T>* node = reinterpret_cast<ObjectPoolNode<T>*>(reinterpret_cast<char*>(object) - offsetof(ObjectPoolNode<T>, _data));
    node->next = _top;
    _top = node;
    return true;
#endif
}

//////////////////////////////////////////// 미완성 ////////////////////////////////////////////
template<typename T>
bool ObjectMemoryPool<T>::Free_Destructor(T* object)
{
#ifdef _DEBUG
    // object는 _data를 가리키고 있으므로 ObjectPoolNode<T>로 변환
    ObjectPoolNode<T>* node = reinterpret_cast<ObjectPoolNode<T>*>(reinterpret_cast<char*>(object) - offsetof(ObjectPoolNode<T>, _data));

    // 가드 값 검사
    if (node->guard1 != GUARD1 || node->guard2 != GUARD2)
    {
        DebugBreak(); // 가드 값이 맞지 않으면 디버그 브레이크
        return false;
    }
    node->_data.~T();
    node->next = _top;
    _top = node;
    return true;
#else
    // object의 메모리를 풀이 가리키는 _top으로 돌려놓음
    ObjectPoolNode<T>* node = reinterpret_cast<ObjectPoolNode<T>*>(reinterpret_cast<char*>(object) - offsetof(ObjectPoolNode<T>, _data));
    node->_data.~T();
    node->next = _top;
    _top = node;
    return true;
#endif

}
//////////////////////////////////////////// 미완성 ////////////////////////////////////////////