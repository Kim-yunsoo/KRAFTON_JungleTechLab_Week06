#pragma once
#include "Actor.h"

class UHeightFogComponent;

/**
 * AExponentialHeightFogActor
 * - Exponential Height Fog를 씬에 배치하기 위한 Actor
 * - UHeightFogComponent를 소유하고 관리
 */
class AExponentialHeightFogActor : public AActor
{
public:
    DECLARE_CLASS(AExponentialHeightFogActor, AActor)

    AExponentialHeightFogActor();
    virtual ~AExponentialHeightFogActor() override;

    virtual void Tick(float DeltaTime) override;

    // Component 접근자
    UHeightFogComponent* GetHeightFogComponent() const { return HeightFogComponent; }
    void SetHeightFogComponent(UHeightFogComponent* InHeightFogComponent);

    // Scene 로드 시 생성자가 만든 기본 컴포넌트를 삭제하기 위한 메서드
    void ClearDefaultComponents();

    // Component 삭제 제한 (HeightFogComponent는 삭제 불가)
    virtual bool DeleteComponent(USceneComponent* ComponentToDelete) override;

    // Duplication (PIE 모드 지원)
    virtual UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
    virtual void DuplicateSubObjects() override;

protected:
    // Height Fog Component (RootComponent로도 사용)
    UHeightFogComponent* HeightFogComponent = nullptr;
};