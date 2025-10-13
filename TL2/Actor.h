#pragma once
#include "Enums.h"
#include "Object.h"
#include "Vector.h"

class UWorld;
class USceneComponent;
class UAABoundingBoxComponent;
class UShapeComponent;

class AActor : public UObject
{
public:
    DECLARE_CLASS(AActor, UObject)
    AActor(); 

protected:
    ~AActor() override;

public:
    virtual void BeginPlay();
    virtual void Tick(float DeltaSeconds);
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

    virtual void Destroy();

    // ───────────────
    // Transform API
    // ───────────────
    void SetActorTransform(const FTransform& InNewTransform) const;
    FTransform GetActorTransform() const;   

    void SetActorLocation(const FVector& InNewLocation);
    FVector GetActorLocation() const;

    void SetActorRotation(const FVector& InEulerDegree) const;
    void SetActorRotation(const FQuat& InQuat) const;
    FQuat GetActorRotation() const;

    void SetActorScale(const FVector& InNewScale) const;
    FVector GetActorScale() const;

    FMatrix GetWorldMatrix() const;

    FVector GetActorForward() const { return GetActorRotation().RotateVector(FVector(0, 1, 0)); }
    FVector GetActorRight()   const { return GetActorRotation().RotateVector(FVector(1, 0, 0)); }
    FVector GetActorUp()      const { return GetActorRotation().RotateVector(FVector(0, 0, 1)); }

    void AddActorWorldRotation(const FQuat& InDeltaRotation) const;
    void AddActorWorldLocation(const FVector& DeltaRot) const;

    void AddActorLocalRotation(const FQuat& InDeltaRotation) const;
    void AddActorLocalLocation(const FVector& DeltaRot) const;

    USceneComponent* GetRootComponent() { return RootComponent; }

    void SetIsPicked(bool picked) { bIsPicked = picked; }
    bool GetIsPicked() { return bIsPicked; }

    

    //-----------------------------
    //----------Getter------------
    const TSet<UActorComponent*>& GetComponents() const;

    void SetName(const FString& InName) { Name = InName; }
    const FName& GetName() const { return Name; }

    template<typename T>
    T* CreateDefaultSubobject(const FName& SubobjectName)
    {
        // NewObject를 통해 생성
        T* Comp = ObjectFactory::NewObject<T>();
        Comp->SetOwner(this);
       // Comp->SetName(SubobjectName);  //나중에 추가 구현
        AddComponent(Comp);
        return Comp;
    }

    template<typename T>
    TSet<T*> GetComponents()
    {
        TSet<T*> Set;
        T* Component = nullptr;
        for (UActorComponent* Child : OwnedComponents)
        {
            if (Component = Cast<T>(Child))
            {
                Set.Add(Component);
            }
        }
        return Set;
    }

    USceneComponent* CreateAndAttachComponent(USceneComponent* ParentComponent, UClass* ComponentClass);
    UActorComponent* CreateActorComponent(UClass* ComponentClass);

    virtual bool DeleteComponent(USceneComponent* ComponentToDelete);
    virtual bool DeleteActorComponent(UActorComponent* ComponentToDelete);

    void AddComponent(USceneComponent* InComponent);

    // Duplicate function
    UObject* Duplicate() override;
    UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
    void DuplicateSubObjects() override;

public:

    // Visibility properties
    void SetActorHiddenInGame(bool bNewHidden) { bHiddenInGame = bNewHidden; }
    bool GetActorHiddenInGame() const { return bHiddenInGame; }
    bool IsActorVisible() const { return !bHiddenInGame; }
    
    
    // Tick Enabled Check
    bool IsActorTickEnabled() const { return bCanEverTick; }

    // Tick 조건 헬퍼 함수들
    bool ShouldTickInEditor() const { return bTickInEditor; }
    bool CanTickInPlayMode() const { return bCanEverTick && !bHiddenInGame; }

    


    UWorld* GetWorld() const override final;
    // TODO(KHJ): 제거 필요
    void SetWorld(UWorld* InWorld) { World = InWorld; }

public:
    // NOTE: UObject의 ObjectName과 용도가 겹치는 것 같음?
    // [PIE] 값 복사
    FName Name;

    // [PIE] Duplicate 복사
    USceneComponent* RootComponent = nullptr;
    // [PIE] RootComponent 복사가 끝나면 자식 컴포넌트를 순회하면서 OwnedComponents에 루트와 하위 컴포넌트 모두 추가
    TSet<UActorComponent*> OwnedComponents;
    
	// Deprecated: Actor가 직접 CollisionComponent를 가지지 않음
    UAABoundingBoxComponent* CollisionComponent = nullptr;

    // [PIE] 외부에서 초기화, Level로 변경 필요?
    UWorld* World = nullptr;

protected:
    // [PIE] 값 복사
    bool bIsPicked = false;
    bool bCanEverTick = true;
    bool bHiddenInGame = false;
    bool bTickInEditor = false;
};
