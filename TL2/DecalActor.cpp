#include "pch.h"
#include "DecalActor.h"
#include "ObjectFactory.h"

ADecalActor::ADecalActor()
{
    // AActor의 생성자에서 기본 USceneComponent를 Root로 생성하지만
    // DecalActor는 DecalComponent를 Root로 사용해야 하므로 기존 Root를 제거 후 재할당
    if (RootComponent)
    {
        USceneComponent* TempRootComponent = RootComponent;
        RootComponent = nullptr;
        DeleteComponent(TempRootComponent);
    }

    Name = "Decal Actor";
    DecalComponent = CreateDefaultSubobject<UDecalComponent>("DecalComponent");
    RootComponent = DecalComponent;
    AddComponent(DecalComponent);
}

ADecalActor::~ADecalActor()
{
    if (DecalComponent)
    {
        ObjectFactory::DeleteObject(DecalComponent);
    }
    DecalComponent = nullptr;
}

void ADecalActor::Tick(float DeltaTime)
{
    Super_t::Tick(DeltaTime);

    // Fade 업데이트
    if (DecalComponent)
    {
        DecalComponent->UpdateFade(DeltaTime);
    }
}

void ADecalActor::SetDecalComponent(UDecalComponent* InDecalComponent)
{
    DecalComponent = InDecalComponent;
}

// Fade Control Interface
void ADecalActor::StartFadeIn(float Duration)
{
    if (DecalComponent)
    {
        DecalComponent->StartFadeIn(Duration);
    }
}

void ADecalActor::StartFadeOut(float Duration)
{
    if (DecalComponent)
    {
        DecalComponent->StartFadeOut(Duration);
    }
}

void ADecalActor::StopFade()
{
    if (DecalComponent)
    {
        // Fade 중단: 현재 상태를 None으로 설정
        DecalComponent->StartFadeIn(0.0f); // Duration=0으로 즉시 완료
    }
}

// Fade State Query
bool ADecalActor::IsFading() const
{
    return DecalComponent ? DecalComponent->IsFading() : false;
}

EDecalFadeState::Type ADecalActor::GetFadeState() const
{
    return DecalComponent ? DecalComponent->GetFadeState() : EDecalFadeState::None;
}

float ADecalActor::GetCurrentOpacity() const
{
    return DecalComponent ? DecalComponent->GetCurrentOpacity() : 0.0f;
}

// Decal Properties
void ADecalActor::SetDecalTexture(const FString& TexturePath)
{
    if (DecalComponent)
    {
        DecalComponent->SetDecalTexture(TexturePath);
    }
}

void ADecalActor::SetDecalSize(const FVector& InSize)
{
    if (DecalComponent)
    {
        DecalComponent->SetDecalSize(InSize);
    }
}

void ADecalActor::SetBaseOpacity(float InOpacity)
{
    if (DecalComponent)
    {
        DecalComponent->SetBaseOpacity(InOpacity);
    }
}

// Duplicate
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

void ADecalActor::DuplicateSubObjects()
{
    // Duplicate()에서 이미 RootComponent를 복제했으므로
    // 부모 클래스가 OwnedComponents를 재구성
    Super_t::DuplicateSubObjects();

    // 타입별 포인터 재설정
    DecalComponent = Cast<UDecalComponent>(RootComponent);
}
