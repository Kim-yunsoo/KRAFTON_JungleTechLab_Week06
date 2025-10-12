// DecalActor.cpp
#include "pch.h"
#include "DecalActor.h"
#include "OBoundingBoxComponent.h"
#include "AABoundingBoxComponent.h"
#include "DecalComponent.h"
#include "ObjectFactory.h"
#include "BillboardComponent.h"


ADecalActor::ADecalActor()
{
    // DecalComponent 생성 및 컴포넌트 생성
    DecalComponent = CreateDefaultSubobject<UDecalComponent>(FName("DecalComponent")); 
    DecalComponent->SetupAttachment(RootComponent);

    BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(FName("BillboardComponent"));
    BillboardComponent->SetWorldLocation(FVector(0.f, 0.f, 0.f));
    BillboardComponent->SetupAttachment(RootComponent);
}

ADecalActor::~ADecalActor()
{
}

void ADecalActor::Tick(float DeltaTime)
{
    Super_t::Tick(DeltaTime);

    // Fade 업데이트
    if (DecalComponent)
    {
        DecalComponent->DecalAnimTick(DeltaTime);
    }
}

void ADecalActor::SetDecalComponent(UDecalComponent* InDecalComponent)
{
    DecalComponent = InDecalComponent;
    DecalComponent->SetupAttachment(RootComponent);
}

bool ADecalActor::DeleteComponent(USceneComponent* ComponentToDelete)
{
    if (ComponentToDelete == DecalComponent)
    {
        // 디칼 컴포넌트는 삭제할 수 없음
        return false;
    }
    return AActor::DeleteComponent(ComponentToDelete);
}

UObject* ADecalActor::Duplicate()
{
    // 원본(this)의 컴포넌트들 저장
    USceneComponent* OriginalRoot = this->RootComponent;

    // 얕은 복사 수행 (생성자 실행됨 - DecalComponent 생성)
    ADecalActor* DuplicatedActor = NewObject<ADecalActor>(*this);

    // 생성자가 만든 컴포넌트 삭제
    if (DuplicatedActor->DecalComponent)
    {
        DuplicatedActor->OwnedComponents.Remove(DuplicatedActor->DecalComponent);
        ObjectFactory::DeleteObject(DuplicatedActor->DecalComponent);
        DuplicatedActor->DecalComponent = nullptr; 
    }

    if (DuplicatedActor->BillboardComponent)
    {
        DuplicatedActor->OwnedComponents.Remove(DuplicatedActor->BillboardComponent);
        ObjectFactory::DeleteObject(DuplicatedActor->BillboardComponent);
        DuplicatedActor->BillboardComponent = nullptr;
    }

    DuplicatedActor->RootComponent = nullptr;
    DuplicatedActor->OwnedComponents.clear();

    // 원본의 RootComponent(DecalComponent) 복제
    if (OriginalRoot)
    {
        DuplicatedActor->RootComponent = Cast<USceneComponent>(OriginalRoot->Duplicate());
    }

    // OwnedComponents 재구성 및 타입별 포인터 재설정
    DuplicatedActor->DuplicateSubObjects();

    return DuplicatedActor;
}

UObject* ADecalActor::Duplicate(FObjectDuplicationParameters Parameters)
{
    auto DupObject = static_cast<ADecalActor*>(Super_t::Duplicate(Parameters));

    // Locate duplicated components and wire pointers
    //DupActor->DecalComponent = nullptr;
    //DupActor->BillboardComponent = nullptr;

    for (UActorComponent* Component : OwnedComponents)
    {
        if (RootComponent == Component)
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

void ADecalActor::DuplicateSubObjects()
{
    // Duplicate()에서 이미 RootComponent를 복제했으므로
    // 부모 클래스가 OwnedComponents를 재구성
    Super_t::DuplicateSubObjects();

    // 타입별 포인터 재설정
    DecalComponent = Cast<UDecalComponent>(RootComponent);

    // DecalComponent 찾기
    for (UActorComponent* Comp : OwnedComponents)
    {
        if (UDecalComponent* Decal = Cast<UDecalComponent>(Comp))
        {
            DecalComponent = Decal;
            break;
        }
    } 
}
