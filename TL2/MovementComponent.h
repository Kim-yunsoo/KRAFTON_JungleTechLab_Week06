#pragma once
#include "ActorComponent.h"
#include "Vector.h"

class USceneComponent;

/**
 * UMovementComponent
 * 이동 로직을 처리하는 컴포넌트의 기본 클래스
 * 기본적인 속도, 가속도, 이동 상태 관리를 제공합니다.
 */
class UMovementComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UMovementComponent, UActorComponent)
    UMovementComponent();

protected:
    ~UMovementComponent() override;

public:
    // ───────────────
    // 라이프사이클
    // ───────────────
    virtual void InitializeComponent() override;
    virtual void TickComponent(float DeltaSeconds) override;

    // ───────────────
    // 속도 API
    // ───────────────
    void SetVelocity(const FVector& NewVelocity);
    FVector GetVelocity() const { return Velocity; }

    // ───────────────
    // 가속도 API
    // ───────────────
    void SetAcceleration(const FVector& NewAcceleration);
    FVector GetAcceleration() const { return Acceleration; }

    // 속도와 가속도를 0으로 설정하여 이동 중지
    virtual void StopMovement();

    // ───────────────
    // 업데이트 대상 컴포넌트
    // ───────────────
    // 이 이동 컴포넌트가 업데이트할 대상 컴포넌트
    void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
    USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }

    // ───────────────
    // 이동 속성
    // ───────────────
    void SetUpdateOnlyIfRendered(bool bNewUpdateOnlyIfRendered) { bUpdateOnlyIfRendered = bNewUpdateOnlyIfRendered; }
    bool GetUpdateOnlyIfRendered() const { return bUpdateOnlyIfRendered; }

    // ───────────────
    // 복제
    // ───────────────
    UObject* Duplicate() override;
    UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
    void DuplicateSubObjects() override;

protected:
    // [PIE] Duplicate 복사 대상
    USceneComponent* UpdatedComponent = nullptr;

    // [PIE] 값 복사
    FVector Velocity;
    FVector Acceleration;
    bool bUpdateOnlyIfRendered = false;
};
