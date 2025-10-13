#pragma once
#include "MovementComponent.h"
#include "Vector.h"

class AActor;
class USceneComponent;

/**
 * UProjectileMovementComponent
 * 발사체(Projectile)의 움직임을 시뮬레이션하는 컴포넌트
 * 중력, 바운스, 호밍 등의 기능을 지원
 */
class UProjectileMovementComponent : public UMovementComponent
{
public:
    DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)
    UProjectileMovementComponent();

protected:
    ~UProjectileMovementComponent() override;

public:
    // Life Cycle
    virtual void TickComponent(float DeltaSeconds) override;

    // 발사 API
    void FireInDirection(const FVector& ShootDirection);
    void SetVelocityInLocalSpace(const FVector& NewVelocity);

    // 물리 속성 Getter/Setter
    void SetGravity(const FVector& NewGravity) { Gravity = NewGravity; }
    FVector GetGravity() const { return Gravity; }

    void SetInitialSpeed(float NewInitialSpeed) { InitialSpeed = NewInitialSpeed; }
    float GetInitialSpeed() const { return InitialSpeed; }

    void SetMaxSpeed(float NewMaxSpeed) { MaxSpeed = NewMaxSpeed; }
    float GetMaxSpeed() const { return MaxSpeed; }

    // 바운스 속성 Getter/Setter
    void SetBounciness(float NewBounciness) { Bounciness = FMath::Clamp(NewBounciness, 0.0f, 1.0f); }
    float GetBounciness() const { return Bounciness; }

    void SetFriction(float NewFriction) { Friction = FMath::Clamp(NewFriction, 0.0f, 1.0f); }
    float GetFriction() const { return Friction; }

    void SetShouldBounce(bool bNewShouldBounce) { bShouldBounce = bNewShouldBounce; }
    bool ShouldBounce() const { return bShouldBounce; }

    void SetMaxBounces(int32 NewMaxBounces) { MaxBounces = NewMaxBounces; }
    int32 GetMaxBounces() const { return MaxBounces; }

    int32 GetCurrentBounceCount() const { return CurrentBounceCount; }

    // 호밍 속성 Getter/Setter
    void SetHomingTarget(AActor* Target);
    void SetHomingTarget(USceneComponent* Target);
    AActor* GetHomingTargetActor() const { return HomingTargetActor; }
    USceneComponent* GetHomingTargetComponent() const { return HomingTargetComponent; }

    void SetHomingAccelerationMagnitude(float NewMagnitude) { HomingAccelerationMagnitude = NewMagnitude; }
    float GetHomingAccelerationMagnitude() const { return HomingAccelerationMagnitude; }

    void SetIsHomingProjectile(bool bNewIsHoming) { bIsHomingProjectile = bNewIsHoming; }
    bool IsHomingProjectile() const { return bIsHomingProjectile; }

    // 회전 속성 Getter/Setter
    void SetRotationFollowsVelocity(bool bNewRotationFollows) { bRotationFollowsVelocity = bNewRotationFollows; }
    bool GetRotationFollowsVelocity() const { return bRotationFollowsVelocity; }

    // 생명주기 Getter/Setter
    void SetProjectileLifespan(float NewLifespan) { ProjectileLifespan = NewLifespan; }
    float GetProjectileLifespan() const { return ProjectileLifespan; }

    void SetAutoDestroyWhenLifespanExceeded(bool bNewAutoDestroy) { bAutoDestroyWhenLifespanExceeded = bNewAutoDestroy; }
    bool GetAutoDestroyWhenLifespanExceeded() const { return bAutoDestroyWhenLifespanExceeded; }

    // 상태 API
    void SetActive(bool bNewActive) { bIsActive = bNewActive; }
    bool IsActive() const { return bIsActive; }

    void ResetLifetime() { CurrentLifetime = 0.0f; }
    float GetCurrentLifetime() const { return CurrentLifetime; }

    // 복제
    UObject* Duplicate() override;
    UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
    void DuplicateSubObjects() override;

protected:
    // 내부 헬퍼 함수
    void LimitVelocity();
    void HandleBounce(const FVector& HitNormal, const FVector& HitLocation);
    void ComputeHomingAcceleration(float DeltaTime);
    void UpdateRotationFromVelocity();

protected:
    // [PIE] 값 복사

    // === 물리 속성 ===
    // 중력 가속도 (cm/s^2), Z-Up 좌표계에서 Z가 음수면 아래로 떨어짐
    FVector Gravity;

    // 발사 시 초기 속도 (cm/s)
    float InitialSpeed;

    // 최대 속도 제한 (cm/s), 0이면 제한 없음
    float MaxSpeed;

    // === 바운스 속성 ===
    // 반발 계수 (0~1), 1이면 완전 탄성 충돌
    float Bounciness;

    // 마찰 계수 (0~1), 바운스 시 속도 감소
    float Friction;

    // 바운스 기능 활성화 여부
    bool bShouldBounce;

    // 최대 바운스 횟수 (0이면 무제한)
    int32 MaxBounces;

    // 현재 바운스 횟수
    int32 CurrentBounceCount;

    // === 호밍 속성 ===
    // 호밍 타겟 액터
    AActor* HomingTargetActor;

    // 호밍 타겟 컴포넌트 (우선순위가 더 높음)
    USceneComponent* HomingTargetComponent;

    // 호밍 가속도 크기 (cm/s^2)
    float HomingAccelerationMagnitude;

    // 호밍 기능 활성화 여부
    bool bIsHomingProjectile;

    // === 회전 속성 ===
    // 속도 방향으로 회전 여부
    bool bRotationFollowsVelocity;

    // === 생명주기 속성 ===
    // 발사체 생명 시간 (초), 0이면 무제한
    float ProjectileLifespan;

    // 현재 생존 시간 (초)
    float CurrentLifetime;

    // 생명 시간 초과 시 자동 파괴 여부
    bool bAutoDestroyWhenLifespanExceeded;

    // === 상태 ===
    // 활성화 상태
    bool bIsActive;
};
