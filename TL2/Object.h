#pragma once
#include "UEContainer.h"
#include "ObjectFactory.h"
#include "MemoryManager.h"
#include "Name.h"
#include "Enums.h"
#include "PropertyFlag.h"

// 전방 선언/외부 심볼 (네 프로젝트 환경 유지)
class UObject;
class UWorld;
// ── UClass: 간단한 타입 디스크립터 ─────────────────────────────
struct UClass
{
    typedef UObject* (*ClassConstructorType)();

    const char* Name = nullptr;
    const UClass* Super = nullptr;   // 루트(UObject)는 nullptr
    std::size_t   Size = 0;

    constexpr UClass() = default;
    constexpr UClass(const char* n, const UClass* s, std::size_t z)//언리얼도 런타임 시간에 관리해주기 때문에 문제가 없습니다.
        :Name(n), Super(s), Size(z) {
    }
    bool IsChildOf(const UClass* Base) const noexcept
    {
        if (!Base) return false;
        for (auto c = this; c; c = c->Super)
            if (c == Base) return true;
        return false;
    }

    UObject* CreateDefaultObject() const; 
private:
    ClassConstructorType Constructor;
};

/** StaticDuplicateObject()와 관련 함수에서 사용되는 Enum */
namespace EDuplicateMode
{
    enum Type
    {
        /** 복제에 구체적인 정보가 없는경우 */
        Normal,
        /** 월드 복제의 일부로 오브젝트가 복제되는 경우 */
        World,
        /** PIE(Play In Editor)를 위해 오브젝트가 복사되는 경우 */
        PIE
    };
}

struct FObjectDuplicationParameters
{
    /** 복사될 오브젝트 */
    UObject* SourceObject;

    /** 복사될 오브젝트를 사용할 오브젝트*/
    UObject* DestOuter;

    /** SourceObject의 복제에 사용될 이름 */
    FName DestName;

    /** 복제될 클래스의 타입 */
    UClass* DestClass;

    EDuplicateMode::Type DuplicateMode;
    
    /**
     * StaticDuplicateObject에서 사용하는 복제 매핑 테이블을 미리 채워넣기 위한 용도.
     * 특정 오브젝트를 복제하지 않고, 이미 존재하는 다른 오브젝트를 그 복제본으로 사용하고 싶을 때 활용한다.
     *
     * 이 맵에 들어간 오브젝트들은 실제로 복제되지 않는다.
     * Key는 원본 오브젝트, Value는 복제본으로 사용될 오브젝트이다.
     */
    TMap<UObject*, UObject*>& DuplicationSeed;
  
    /**
     * null이 아니라면, StaticDuplicateObject 호출 시 생성된 모든 복제 오브젝트들이 이 맵에 기록된다.
     *
     * Key는 원본 오브젝트, Value는 새로 생성된 복제 오브젝트이다.
     * DuplicationSeed에 의해 미리 매핑된 오브젝트들은 여기에는 들어가지 않는다.
     */
    TMap<UObject*, UObject*>& CreatedObjects;

    /** 생성자 */
    FObjectDuplicationParameters(UObject* InSourceObject, UObject* InDestOuter, TMap<UObject*, UObject*>& InDuplicationSeed, TMap<UObject*, UObject*>& InCreatedObjects)
        : SourceObject(InSourceObject), DestOuter(InDestOuter), DuplicationSeed(InDuplicationSeed), CreatedObjects(InCreatedObjects)
    {

    } 
};
class UObject
{
public:
    UObject() : UUID(GenerateUUID()), InternalIndex(UINT32_MAX), ObjectName("UObject"), Outer(nullptr) {}

protected:
    virtual ~UObject() = default;
    // Centralized deletion entry accessible to ObjectFactory only
    void DestroyInternal() { delete this; }
    friend void ObjectFactory::DeleteObject(UObject* Obj);

public:
    // UObject-scoped allocation only
    static void* operator new(std::size_t size) { return CMemoryManager::Allocate(size); }
    static void  operator delete(void* ptr) noexcept { CMemoryManager::Deallocate(ptr); }
    static void  operator delete(void* ptr, std::size_t) noexcept { CMemoryManager::Deallocate(ptr); }

    FString GetName();    // 원문
    FString GetComparisonName(); // lower-case

    UObject* GetOuter() const;
    void SetOuter(UObject* InObject);

    virtual UWorld* GetWorld() const;
      
    virtual UObject* Duplicate(FObjectDuplicationParameters Parameters);
    virtual UObject* Duplicate();
    virtual void DuplicateSubObjects();
     

public:

    // [PIE] ???
    uint32_t UUID;
    uint32_t InternalIndex;
    // [PIE] 값 복사
    FName    ObjectName;   // ← 객체 개별 이름 추가
    UObject* Outer = nullptr;  // ← Outer 객체 참조

public:
    // 정적: 타입 메타 반환 (이름을 StaticClass로!)
    static UClass* StaticClass()
    {
        static UClass Cls{ "UObject", nullptr, sizeof(UObject) };
        return &Cls;
    }

    // 가상: 인스턴스의 실제 타입 메타
    virtual UClass* GetClass() const { return StaticClass(); }

    // IsA 헬퍼
    bool IsA(const UClass* C) const noexcept { return GetClass()->IsChildOf(C); }
    template<class T> bool IsA() const noexcept { return IsA(T::StaticClass()); }

    // 다음으로 발급될 UUID를 조회 (증가 없음)
    static uint32 PeekNextUUID() { return GUUIDCounter; }

    // 다음으로 발급될 UUID를 설정 (예: 씬 로드시 메타와 동기화)
    static void SetNextUUID(uint32 Next) { GUUIDCounter = Next; }

    // UUID 발급기: 현재 카운터를 반환하고 1 증가
    static uint32 GenerateUUID() { return GUUIDCounter++; }
    
    //static EPropertyFlag GetPropertyFlag() { return EPropertyFlag::CPF_Instanced; }

private:
    // 전역 UUID 카운터(초기값 1)
    inline static uint32 GUUIDCounter = 1;
};

// ── Cast 헬퍼 (UE Cast<> 와 동일 UX) ────────────────────────────
template<class T>
T* Cast(UObject* Obj) noexcept
{
    return (Obj && Obj->IsA<T>()) ? static_cast<T*>(Obj) : nullptr;
}
template<class T>
const T* Cast(const UObject* Obj) noexcept
{
    return (Obj && Obj->IsA<T>()) ? static_cast<const T*>(Obj) : nullptr;
}

// ── 파생 타입에 붙일 매크로 ─────────────────────────────────────
#define DECLARE_CLASS(ThisClass, SuperClass)                                  \
public:                                                                       \
    using Super_t = SuperClass;                                               \
    static UClass* StaticClass()                                              \
    {                                                                         \
        static UClass Cls{ #ThisClass, SuperClass::StaticClass(),             \
                            sizeof(ThisClass) };                              \
        return &Cls;                                                          \
    }                                                                         \
    virtual UClass* GetClass() const override { return ThisClass::StaticClass(); }


// ㅡㅡ Duplication ─────────────────────────────────────

inline FObjectDuplicationParameters InitStaticDuplicateObjectParams(UObject const* SourceObject, UObject* DestOuter, const FName DestName,
    TMap<UObject*, UObject*>& DuplicationSeed, TMap<UObject*, UObject*>& CreatedObjects, EDuplicateMode::Type DuplicateMode = EDuplicateMode::Normal) 
{
    FObjectDuplicationParameters Parameters(const_cast<UObject*>(SourceObject), DestOuter, DuplicationSeed, CreatedObjects);
    if (DestName != FName::GetNone())
    {
        Parameters.DestName = DestName;
    }

    Parameters.DestClass = SourceObject->GetClass();
    Parameters.DuplicateMode = DuplicateMode;

    return Parameters;
}
 
//template<typename T> 
//T* UObject::Duplicate()
//{
//    T* NewObject;
//    
//    EPropertyFlag EffectiveFlags = T::GetPropertyFlag();
//    
//    if ((EffectiveFlags & EPropertyFlag::CPF_DuplicateTransient) != EPropertyFlag::CPF_None)
//    {
//        // New Instance: 기본 생성자로 새 객체 생성
//        NewObject = new T();
//    }
//    else if ((EffectiveFlags & EPropertyFlag::CPF_EditAnywhere) != EPropertyFlag::CPF_None)
//    {
//        // Shallow Copy: 에디터에서 편집 가능한 얕은 복사
//        NewObject = new T(*static_cast<T*>(this));
//    }
//    else if ((EffectiveFlags & EPropertyFlag::CPF_Instanced) != EPropertyFlag::CPF_None)
//    {
//        // Deep Copy: 참조된 객체도 Deep Copy
//        NewObject = new T(*static_cast<T*>(this));
//        NewObject->DuplicateSubObjects();
//    }
//    else // 기본값: Deep Copy
//    {
//        NewObject = new T(*static_cast<T*>(this));
//        NewObject->DuplicateSubObjects();
//    }
//    
//    return NewObject;
//} 