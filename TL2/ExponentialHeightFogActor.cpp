#include "pch.h"
#include "ExponentialHeightFogActor.h"
#include "HeightFogComponent.h"
#include "ObjectFactory.h"

AExponentialHeightFogActor::AExponentialHeightFogActor()
{
    // HeightFogComponent 생성 및 RootComponent로 설정
    HeightFogComponent = CreateDefaultSubobject<UHeightFogComponent>(FName("HeightFogComponent"));
    RootComponent = HeightFogComponent;

    // 기본 위치 설정 (씬 중심, 지면 레벨)
    SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
}

AExponentialHeightFogActor::~AExponentialHeightFogActor()
{
    // HeightFogComponent 정리 (ObjectFactory가 관리)
    if (HeightFogComponent)
    {
        ObjectFactory::DeleteObject(HeightFogComponent);
    }
    HeightFogComponent = nullptr;
}

void AExponentialHeightFogActor::Tick(float DeltaTime)
{
    Super_t::Tick(DeltaTime);

    // Height Fog는 매 프레임 업데이트가 필요 없으므로 비워둠
    // 필요시 동적 안개 효과 추가 가능 (예: 시간대별 안개 밀도 변화)
}

void AExponentialHeightFogActor::SetHeightFogComponent(UHeightFogComponent* InHeightFogComponent)
{
    HeightFogComponent = InHeightFogComponent;

    // RootComponent가 HeightFogComponent와 다를 때만 Attach
    if (HeightFogComponent && RootComponent && HeightFogComponent != RootComponent)
    {
        HeightFogComponent->SetupAttachment(RootComponent);
    }
}

void AExponentialHeightFogActor::ClearDefaultComponents()
{
    // 생성자가 만든 HeightFogComponent를 삭제 (Scene 로드 시 사용)
    if (HeightFogComponent)
    {
        OwnedComponents.Remove(HeightFogComponent);
        ObjectFactory::DeleteObject(HeightFogComponent);
        HeightFogComponent = nullptr;
        RootComponent = nullptr;
    }
}

bool AExponentialHeightFogActor::DeleteComponent(USceneComponent* ComponentToDelete)
{
    // HeightFogComponent는 삭제 불가 (Actor의 핵심 컴포넌트)
    if (ComponentToDelete == HeightFogComponent)
    {
        return false;
    }

    // 다른 컴포넌트는 부모 클래스에서 처리
    return Super_t::DeleteComponent(ComponentToDelete);
}

UObject* AExponentialHeightFogActor::Duplicate(FObjectDuplicationParameters Parameters)
{
    // 부모 클래스의 Duplicate 호출 (Transform 등 복사)
    auto DupObject = static_cast<AExponentialHeightFogActor*>(Super_t::Duplicate(Parameters));

    // OwnedComponents 순회하여 복제
    for (UActorComponent* Component : OwnedComponents)
    {
        // RootComponent는 이미 처리됨
        if (RootComponent == Component)
        {
            continue;
        }

        // DuplicationSeed에서 이미 복제된 컴포넌트 찾기
        if (auto It = Parameters.DuplicationSeed.find(Component); It != Parameters.DuplicationSeed.end())
        {
            DupObject->OwnedComponents.emplace(static_cast<UActorComponent*>(It->second));
        }
        else
        {
            // 새로 복제
            auto Params = InitStaticDuplicateObjectParams(
                Component,
                DupObject,
                FName::GetNone(),
                Parameters.DuplicationSeed,
                Parameters.CreatedObjects
            );
            auto DupComponent = static_cast<UActorComponent*>(Component->Duplicate(Params));

            DupObject->OwnedComponents.emplace(DupComponent);
        }
    }

    return DupObject;
}

void AExponentialHeightFogActor::DuplicateSubObjects()
{
    // 부모 클래스가 OwnedComponents를 재구성
    Super_t::DuplicateSubObjects();

    // 타입별 포인터 재설정: RootComponent → HeightFogComponent
    HeightFogComponent = Cast<UHeightFogComponent>(RootComponent);

    // 안전성 체크: OwnedComponents에서도 찾기
    if (!HeightFogComponent)
    {
        for (UActorComponent* Comp : OwnedComponents)
        {
            if (UHeightFogComponent* FogComp = Cast<UHeightFogComponent>(Comp))
            {
                HeightFogComponent = FogComp;
                break;
            }
        }
    }
}