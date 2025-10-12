#include "pch.h"
#include "ActorComponent.h"

UActorComponent::UActorComponent()
    : Owner(nullptr), bIsActive(true), bCanEverTick(false)
{
}

UActorComponent::~UActorComponent()
{
}

void UActorComponent::InitializeComponent()
{
    // 액터에 부착될 때 초기화
    // 필요하다면 Override
}

void UActorComponent::BeginPlay()
{
    // 게임 시작 시
    // 필요하다면 Override
}

void UActorComponent::TickComponent(float DeltaSeconds)
{
    if (!bIsActive || !bCanEverTick)
        return;

    // 매 프레임 처리
    // 자식 클래스에서 Override
}

void UActorComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    if (EndPlayReason == EEndPlayReason::EndPlayInEditor)
    {
        // End Replication
    }
}

UObject* UActorComponent::Duplicate()
{
    UActorComponent* DuplicatedComponent = NewObject<UActorComponent>(*this);
    DuplicatedComponent->DuplicateSubObjects();

    return DuplicatedComponent;
}

UObject* UActorComponent::Duplicate(FObjectDuplicationParameters Parameters)
{
    auto DupObject = static_cast<UActorComponent*>(Super_t::Duplicate(Parameters));
     
    //Owner 처리
    if (Owner)
    {
        // Deuplicat할 Component 체크      
        if (auto It = Parameters.DuplicationSeed.find(Owner); It != Parameters.DuplicationSeed.end() )
        {
            DupObject->Owner = static_cast<AActor*>(It->second);
        }
        else
        { 
            /** @todo 플래그를 도입해서 위쪽 계층이 반영될 수 있도록 변경한다. */
            UE_LOG("Owner는 UActorComponent보다 먼저 생성되어야 합니다.");
            DupObject->Owner = nullptr;
        }
    }
    else
    {
        DupObject->Owner = nullptr;
    }

    return DupObject;
}

void UActorComponent::DuplicateSubObjects()
{
    Super_t::DuplicateSubObjects();


}
