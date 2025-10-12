#pragma once
#include "Vector.h"
#include "ActorComponent.h"



// 부착 시 로컬을 유지할지, 월드를 유지할지
enum class EAttachmentRule
{
    KeepRelative,
    KeepWorld
};

class URenderer;
class USceneComponent : public UActorComponent
{
public:
    DECLARE_CLASS(USceneComponent, UActorComponent)
    USceneComponent();

protected:
    ~USceneComponent() override;


public:
    // Tick
    virtual void TickComponent(float DeltaSeconds) override;

    // ──────────────────────────────
    // Relative Transform API
    // ──────────────────────────────
    void SetRelativeLocation(const FVector& NewLocation);
    FVector GetRelativeLocation() const;

    void SetRelativeRotation(const FQuat& NewRotation);
    FQuat GetRelativeRotation() const;

    void SetRelativeScale(const FVector& NewScale);
    FVector GetRelativeScale() const;

    void AddRelativeLocation(const FVector& DeltaLocation);
    void AddRelativeRotation(const FQuat& DeltaRotation);
    void AddRelativeScale3D(const FVector& DeltaScale);

    // ──────────────────────────────
    // World Transform API
    // ──────────────────────────────
    FTransform GetWorldTransform() const;
    void SetWorldTransform(const FTransform& W);

    void SetWorldLocation(const FVector& L);
    FVector GetWorldLocation() const;

    void SetWorldRotation(const FQuat& R);
    FQuat GetWorldRotation() const;

    void SetWorldScale(const FVector& S);
    FVector GetWorldScale() const;

    void AddWorldOffset(const FVector& Delta);
    void AddWorldRotation(const FQuat& DeltaRot);
    void SetWorldLocationAndRotation(const FVector& L, const FQuat& R);

    void AddLocalOffset(const FVector& Delta);
    void AddLocalRotation(const FQuat& DeltaRot);
    void SetLocalLocationAndRotation(const FVector& L, const FQuat& R);

    FMatrix GetWorldMatrix() const; // ToMatrixWithScale

    // ──────────────────────────────
    // Attach/Detach
    // ──────────────────────────────
    void SetupAttachment(USceneComponent* InParent, EAttachmentRule Rule = EAttachmentRule::KeepWorld);
    void DetachFromParent(bool bKeepWorld = true);

    // ──────────────────────────────
    // Hierarchy Access
    // ──────────────────────────────
    USceneComponent* GetAttachParent() const { return AttachParent; }
    const TArray<USceneComponent*>& GetAttachChildren() const { return AttachChildren; }
    UWorld* GetWorld() { return AttachParent->GetWorld(); }

    // Duplicate function
    UObject* Duplicate() override;
    UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
    void DuplicateSubObjects() override;

protected:
    void UpdateRelativeTransform();

    // Duplicate 헬퍼: 공통 속성 복사 (Transform, AttachChildren)
    void CopyCommonProperties(USceneComponent* Target);

protected:
    // [PIE] 값 복사
    FVector RelativeLocation{ 0,0,0 };
    FQuat   RelativeRotation;
    FVector RelativeScale{ 1,1,1 };

    // Hierarchy
    // [PIE] 외부에서 초기화
    USceneComponent* AttachParent = nullptr;
    // [PIE] 직접 순회를 돌면서 Duplicate 복사
    TArray<USceneComponent*> AttachChildren;

    // 로컬(부모 기준) 트랜스폼
    // [PIE] 값 복사
    FTransform RelativeTransform;
};
