#include "pch.h"
#include "Actor.h"
#include "SceneComponent.h"
#include "ObjectFactory.h"
#include "ShapeComponent.h"
#include "AABoundingBoxComponent.h"
#include "MeshComponent.h"
#include "TextRenderComponent.h"

AActor::AActor()
{
    Name = "DefaultActor";
    RootComponent = CreateDefaultSubobject<USceneComponent>(FName("SceneComponent"));
}

AActor::~AActor()
{
    for (UActorComponent* Comp : OwnedComponents)
    {
        if (Comp)
        {
            ObjectFactory::DeleteObject(Comp);
        }
    }
    OwnedComponents.Empty();
}

void AActor::BeginPlay()
{
}

void AActor::Tick(float DeltaSeconds)
{
    // 소유한 모든 컴포넌트의 Tick 처리
    for (UActorComponent* Component : OwnedComponents)
    {
        if (Component && Component->CanEverTick())
        {
            Component->TickComponent(DeltaSeconds);
        }
    }
}

/**
 * @brief Endplay 전파 함수
 * @param EndPlayReason Endplay 이유, Type에 따른 다른 설정이 가능함
 */
void AActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (UActorComponent* Component : OwnedComponents)
    {
        Component->EndPlay(EndPlayReason);
    }
}

void AActor::Destroy()
{
    if (!bCanEverTick) return;
    // Prefer world-managed destruction to remove from world actor list
    if (GetWorld())
    {
        // Avoid using 'this' after the call
        GetWorld()->DestroyActor(this);
        return;
    }
    // Fallback: directly delete the actor via factory
    ObjectFactory::DeleteObject(this);
}

// ───────────────
// Transform API
// ───────────────
void AActor::SetActorTransform(const FTransform& InNewTransform) const
{
    if (RootComponent)
    {
        RootComponent->SetWorldTransform(InNewTransform);
    }
}


FTransform AActor::GetActorTransform() const
{
    return RootComponent ? RootComponent->GetWorldTransform() : FTransform();
}

void AActor::SetActorLocation(const FVector& InNewLocation)
{
    if (RootComponent)
    {
        RootComponent->SetWorldLocation(InNewLocation);
    }
}

FVector AActor::GetActorLocation() const
{
    return RootComponent ? RootComponent->GetWorldLocation() : FVector();
}

void AActor::SetActorRotation(const FVector& InEulerDegree) const
{
    if (RootComponent)
    {
        RootComponent->SetWorldRotation(FQuat::MakeFromEuler(InEulerDegree));
    }
}

void AActor::SetActorRotation(const FQuat& InQuat) const
{
    if (RootComponent)
    {
        RootComponent->SetWorldRotation(InQuat);
    }
}

FQuat AActor::GetActorRotation() const
{
    return RootComponent ? RootComponent->GetWorldRotation() : FQuat();
}

void AActor::SetActorScale(const FVector& InNewScale) const
{
    if (RootComponent)
    {
        RootComponent->SetWorldScale(InNewScale);
    }
}

FVector AActor::GetActorScale() const
{
    return RootComponent ? RootComponent->GetWorldScale() : FVector(1, 1, 1);
}

FMatrix AActor::GetWorldMatrix() const
{
    return RootComponent ? RootComponent->GetWorldMatrix() : FMatrix::Identity();
}

void AActor::AddActorWorldRotation(const FQuat& InDeltaRotation) const
{
    if (RootComponent)
    {
        RootComponent->AddWorldRotation(InDeltaRotation);
    }
}

void AActor::AddActorWorldLocation(const FVector& InDeltaLocation) const
{
    if (RootComponent)
    {
        RootComponent->AddWorldOffset(InDeltaLocation);
    }
}

void AActor::AddActorLocalRotation(const FQuat& InDeltaRotation) const
{
    if (RootComponent)
    {
        RootComponent->AddLocalRotation(InDeltaRotation);
    }
}

void AActor::AddActorLocalLocation(const FVector& InDeltaLocation) const
{
    if (RootComponent)
    {
        RootComponent->AddLocalOffset(InDeltaLocation);
    }
}

const TSet<UActorComponent*>& AActor::GetComponents() const
{
    return OwnedComponents;
}

void AActor::AddComponent(USceneComponent* InComponent)
{
    if (!InComponent)
    {
        return;
    }

    OwnedComponents.Add(InComponent);
    if (!RootComponent)
    {
        RootComponent = InComponent;
    }
}

UWorld* AActor::GetWorld() const
{
    // TODO(KHJ): Level 생기면 붙일 것
    // ULevel* Level = GetOuter();
    // if (Level)
    // {
    //     return Level->GetWorld();
    // }

    // return nullptr;

    return World;
}

// ParentComponent 하위에 새로운 컴포넌트를 추가합니다
USceneComponent* AActor::CreateAndAttachComponent(USceneComponent* ParentComponent, UClass* ComponentClass)
{
    // 부모가 지정되지 않았다면 루트 컴포넌트를 부모로 삼습니다.
    if (!ParentComponent)
    {
        ParentComponent = GetRootComponent();
    }

    if (!ComponentClass || !ParentComponent)
    {
        return nullptr;
    }

    // 생성, 등록, 부착 로직을 액터가 직접 책임지고 수행합니다.
    USceneComponent* NewComponent = nullptr;

    if (UObject* NewComponentObject = NewObject(ComponentClass))
    {
        if (NewComponent = Cast<USceneComponent>(NewComponentObject))
        {
            this->AddComponent(NewComponent); // 액터의 관리 목록에 추가

            NewComponent->SetupAttachment(ParentComponent, EAttachmentRule::KeepRelative);
            NewComponent->SetOwner(this);
        }
    }

    return NewComponent;
}

bool AActor::DeleteComponent(USceneComponent* ComponentToDelete)
{
    // 1. [유효성 검사] nullptr이거나 이 액터가 소유한 컴포넌트가 아니면 실패 처리합니다.
    if (!ComponentToDelete || !OwnedComponents.Contains(ComponentToDelete))
    {
        return false;
    }

    // 2. [루트 컴포넌트 보호] 루트 컴포넌트는 액터의 기준점이므로, 직접 삭제하는 것을 막습니다.
    // 루트를 바꾸려면 다른 컴포넌트를 루트로 지정하는 방식을 사용해야 합니다.
    if (ComponentToDelete == RootComponent)
    {
        UE_LOG("루트 컴포넌트는 직접 삭제할 수 없습니다.");
        return false;
    }

    // 3. [자식 컴포넌트 처리] 삭제될 컴포넌트의 자식들을 조부모에게 재연결합니다.
    if (USceneComponent* ParentOfDoomedComponent = ComponentToDelete->GetAttachParent())
    {
        // 자식 목록의 복사본을 만들어 순회합니다. (원본을 수정하면서 순회하면 문제가 발생)
        TArray<USceneComponent*> ChildrenToReAttach = ComponentToDelete->GetAttachChildren();
        for (USceneComponent* Child : ChildrenToReAttach)
        {
            // 자식을 조부모에게 다시 붙입니다.
            Child->SetupAttachment(ParentOfDoomedComponent);
        }
    }

    // 4. [부모로부터 분리] 이제 삭제될 컴포넌트를 부모로부터 분리합니다.
    ComponentToDelete->DetachFromParent();

    // 5. [소유 목록에서 제거] 액터의 관리 목록에서 포인터를 제거합니다.
    //    이걸 하지 않으면 액터 소멸자에서 이미 삭제된 메모리에 접근하여 충돌합니다.
    OwnedComponents.Remove(ComponentToDelete);

    // 6. [메모리 해제] 모든 연결이 정리되었으므로, 마지막으로 객체를 삭제합니다.
    ObjectFactory::DeleteObject(ComponentToDelete);

    return true;
}

UObject* AActor::Duplicate()
{
    return nullptr;
}

UObject* AActor::Duplicate(FObjectDuplicationParameters Parameters)
{
    auto DupObject = static_cast<AActor*>(Super_t::Duplicate(Parameters));

    /** @note 기본 생성자가 생성하는 컴포넌트를 맵에 업데이트 */
    if (RootComponent)
    {
        auto Params = InitStaticDuplicateObjectParams(RootComponent, DupObject, FName::GetNone(), Parameters.DuplicationSeed, Parameters.CreatedObjects);
        Params.DuplicationSeed.Emplace(RootComponent, DupObject->RootComponent);

        RootComponent->Duplicate(Params);
    }
 
    /** @todo 이후 다른 AActor를 상속받는 클래스들이 생성하는 컴포넌트를 어떻게 복제할 것인지 생각 */
    
    for (auto& Component : OwnedComponents)
    {
        // 이미 위에서 처리했음
        if (Component == RootComponent)
        {
            continue;
        }
        
        if (auto It = Parameters.DuplicationSeed.find(Component); It != Parameters.DuplicationSeed.end()) 
        {
            DupObject->OwnedComponents.emplace(static_cast<UActorComponent*>(It->second));
        }
        else
        {
            auto Params = InitStaticDuplicateObjectParams(Component, DupObject, FName::GetNone(), Parameters.DuplicationSeed, Parameters.CreatedObjects);
            auto DupComponent = static_cast<UActorComponent*>(Component->Duplicate(Params));

            DupObject->OwnedComponents.emplace(DupComponent);
        }
    }

    return DupObject;
}

/**
 * @brief Actor의 Internal 복사 함수
 * 원본이 들고 있던 Component를 각 Component의 복사함수를 호출하여 받아온 후 새로 담아서 처리함
 */
void AActor::DuplicateSubObjects()
{
    Super_t::DuplicateSubObjects();

    // Duplicate()에서 이미 RootComponent를 복제했으므로
    // 여기서는 OwnedComponents만 재구성
    if (RootComponent)
    {
        TQueue<USceneComponent*> Queue;
        Queue.Enqueue(RootComponent);
        while (Queue.size() > 0)
        {
            USceneComponent* Component = Queue.front();
            Queue.pop();
            Component->SetOwner(this);
            OwnedComponents.Add(Component);

            for (USceneComponent* Child : Component->GetAttachChildren())
            {
                Queue.Enqueue(Child);
            }
        }
    }

    //TSet<UActorComponent*> DuplicatedComponents = OwnedComponents;
    //OwnedComponents.Empty();
    //
    //USceneComponent* NewRootComponent = nullptr;

    //for (UActorComponent* Component : DuplicatedComponents)
    //{
    //    //USceneComponent* NewComponent = Component->Duplicate<USceneComponent>();
    //    USceneComponent* NewComponent = Cast<USceneComponent>(Component->Duplicate());
    //    
    //    // 복제된 컴포넌트의 Owner를 현재 액터로 설정
    //    if (NewComponent)
    //    {
    //        NewComponent->SetOwner(this);
    //    }
    //    
    //    OwnedComponents.Add(NewComponent);
    //    
    //    if (Component == RootComponent)
    //    {
    //        NewRootComponent = NewComponent;
    //    }
    //}
    //
    //// RootComponent 업데이트
    //RootComponent = NewRootComponent;
}

