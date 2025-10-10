#pragma once
#include "Actor.h"
#include "DecalComponent.h"

// ADecalActor
//  - Decal Component를 가진 Actor
//  - Fade In/Out 제어 인터페이스 제공
//  - Tick에서 Fade 업데이트
class ADecalActor : public AActor
{
public:
    DECLARE_CLASS(ADecalActor, AActor)

    ADecalActor();
    virtual void Tick(float DeltaTime) override;

protected:
    ~ADecalActor() override;

public:
    // Component Access
    UDecalComponent* GetDecalComponent() const { return DecalComponent; }
    void SetDecalComponent(UDecalComponent* InDecalComponent);

    // Fade Control Interface
    void StartFadeIn(float Duration);
    void StartFadeOut(float Duration);
    void StopFade();

    // Fade State Query
    bool IsFading() const;
    EDecalFadeState::Type GetFadeState() const;
    float GetCurrentOpacity() const;

    // Decal Properties (편의 메서드)
    void SetDecalTexture(const FString& TexturePath);
    void SetDecalSize(const FVector& InSize);
    void SetBaseOpacity(float InOpacity);

    // Duplicate Support
    UObject* Duplicate() override;
    void DuplicateSubObjects() override;

protected:
    // [PIE] DecalComponent를 RootComponent로 사용
    UDecalComponent* DecalComponent;
};
