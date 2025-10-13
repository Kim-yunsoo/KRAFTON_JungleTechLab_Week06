#include "pch.h"
#include "RotatingMovementComponent.h"
#include "SceneComponent.h"
#include "ObjectFactory.h"

URotatingMovementComponent::URotatingMovementComponent()
    : RotationRate(0.0f, 0.0f, 0.0f)
    , PivotTranslation(0.0f, 0.0f, 0.0f)
    , bRotationInLocalSpace(true)
{
    bCanEverTick = true;
}

URotatingMovementComponent::~URotatingMovementComponent()
{
}

void URotatingMovementComponent::TickComponent(float DeltaSeconds)
{
    // Editor World에서는 Tick 안함.
    // + GetWorld로 GWorld 받아오면 Dangilng Pointer 버그있음
    if (GWorld->WorldType == EWorldType::Editor)
        return;

    Super_t::TickComponent(DeltaSeconds);

    if (!bIsActive || !bCanEverTick)
        return;

    if (!UpdatedComponent)
        return;

    // 회전 델타 계산 (초당 도 * 델타 시간)
    FVector RotationDelta = RotationRate * DeltaSeconds;

    // 도를 라디안으로 변환하고 쿼터니언 생성
    float PitchRad = RotationDelta.X * (PI / 180.0f);
    float YawRad = RotationDelta.Y * (PI / 180.0f);
    float RollRad = RotationDelta.Z * (PI / 180.0f);

    // 오일러 각도로부터 쿼터니언 생성 (Pitch, Yaw, Roll)
    FQuat DeltaRotation = FQuat::MakeFromEuler(FVector(PitchRad, YawRad, RollRad));

    if (bRotationInLocalSpace)
    {
        // 로컬 공간에서 회전 적용
        if (PivotTranslation.IsNearlyZero())
        {
            // 컴포넌트 원점을 중심으로 단순 회전
            UpdatedComponent->AddLocalRotation(DeltaRotation);
        }
        else
        {
            // 피벗 지점을 중심으로 회전
            FVector CurrentLocation = UpdatedComponent->GetRelativeLocation();
            FQuat CurrentRotation = UpdatedComponent->GetRelativeRotation();

            // 피벗 주변 회전
            FVector OffsetFromPivot = CurrentLocation - PivotTranslation;
            FVector RotatedOffset = DeltaRotation.RotateVector(OffsetFromPivot);
            FVector NewLocation = PivotTranslation + RotatedOffset;

            // 새로운 회전과 위치 적용
            UpdatedComponent->SetRelativeRotation(DeltaRotation * CurrentRotation);
            UpdatedComponent->SetRelativeLocation(NewLocation);
        }
    }
    else
    {
        // 월드 공간에서 회전 적용
        if (PivotTranslation.IsNearlyZero())
        {
            // 컴포넌트 원점을 중심으로 단순 회전
            UpdatedComponent->AddWorldRotation(DeltaRotation);
        }
        else
        {
            // 월드 공간에서 피벗 지점을 중심으로 회전
            FVector WorldPivot = UpdatedComponent->GetWorldLocation() +
                                 UpdatedComponent->GetWorldRotation().RotateVector(PivotTranslation);

            FVector CurrentWorldLocation = UpdatedComponent->GetWorldLocation();
            FQuat CurrentWorldRotation = UpdatedComponent->GetWorldRotation();

            // 월드 피벗 주변 회전
            FVector OffsetFromPivot = CurrentWorldLocation - WorldPivot;
            FVector RotatedOffset = DeltaRotation.RotateVector(OffsetFromPivot);
            FVector NewWorldLocation = WorldPivot + RotatedOffset;

            // 새로운 회전과 위치 적용
            UpdatedComponent->SetWorldRotation(DeltaRotation * CurrentWorldRotation);
            UpdatedComponent->SetWorldLocation(NewWorldLocation);
        }
    }
}

void URotatingMovementComponent::SetRotationRate(const FVector& NewRotationRate)
{
    RotationRate = NewRotationRate;
}

void URotatingMovementComponent::SetPivotTranslation(const FVector& NewPivotTranslation)
{
    PivotTranslation = NewPivotTranslation;
}

void URotatingMovementComponent::SetRotationInLocalSpace(bool bNewRotationInLocalSpace)
{
    bRotationInLocalSpace = bNewRotationInLocalSpace;
}

UObject* URotatingMovementComponent::Duplicate()
{
    URotatingMovementComponent* DuplicatedComponent = NewObject<URotatingMovementComponent>(*this);
    DuplicatedComponent->DuplicateSubObjects();

    return DuplicatedComponent;
}

UObject* URotatingMovementComponent::Duplicate(FObjectDuplicationParameters Parameters)
{
    auto DupObject = static_cast<URotatingMovementComponent*>(Super_t::Duplicate(Parameters));

    // 회전 관련 속성 복사
    DupObject->RotationRate = RotationRate;
    DupObject->PivotTranslation = PivotTranslation;
    DupObject->bRotationInLocalSpace = bRotationInLocalSpace;

    return DupObject;
}

void URotatingMovementComponent::DuplicateSubObjects()
{
    Super_t::DuplicateSubObjects();
    // RotatingMovementComponent는 복제할 하위 오브젝트가 없음
}
