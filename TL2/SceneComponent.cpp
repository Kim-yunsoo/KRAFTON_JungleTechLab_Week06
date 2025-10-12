#include "pch.h"
#include "SceneComponent.h"
#include <algorithm>
#include "ObjectFactory.h"

USceneComponent::USceneComponent()
    : RelativeLocation(0, 0, 0)
    , RelativeRotation(0, 0, 0, 1)
    , RelativeScale(1, 1, 1)
    , AttachParent(nullptr)
{
    UpdateRelativeTransform();
}

USceneComponent::~USceneComponent()
{
    // 자식 메모리 해제
    // 복사본을 만들어 부모 리스트 무효화 문제를 피함
    TArray<USceneComponent*> ChildrenCopy = AttachChildren;
    AttachChildren.clear();
    for (USceneComponent* Child : ChildrenCopy)
    {
        if (Child)
        {
            ObjectFactory::DeleteObject(Child);
        }
    }

    // 부모에서 자신 제거
    if (AttachParent)
    {
        TArray<USceneComponent*>& ParentChildren = AttachParent->AttachChildren;
        ParentChildren.Remove(this);
        AttachParent = nullptr;
    }
}

void USceneComponent::TickComponent(float DeltaSeconds)
{
}

// ──────────────────────────────
// Relative API
// ──────────────────────────────
void USceneComponent::SetRelativeLocation(const FVector& NewLocation)
{
    RelativeLocation = NewLocation;
    UpdateRelativeTransform();
}
FVector USceneComponent::GetRelativeLocation() const { return RelativeLocation; }

void USceneComponent::SetRelativeRotation(const FQuat& NewRotation)
{
    RelativeRotation = NewRotation;
    UpdateRelativeTransform();
}
FQuat USceneComponent::GetRelativeRotation() const { return RelativeRotation; }

void USceneComponent::SetRelativeScale(const FVector& NewScale)
{
    RelativeScale = NewScale;
    UpdateRelativeTransform();
}
FVector USceneComponent::GetRelativeScale() const { return RelativeScale; }

void USceneComponent::AddRelativeLocation(const FVector& DeltaLocation)
{
    RelativeLocation = RelativeLocation + DeltaLocation;
    UpdateRelativeTransform();
}

void USceneComponent::AddRelativeRotation(const FQuat& DeltaRotation)
{
    RelativeRotation = DeltaRotation * RelativeRotation;
    UpdateRelativeTransform();
}

void USceneComponent::AddRelativeScale3D(const FVector& DeltaScale)
{
    RelativeScale = FVector(RelativeScale.X * DeltaScale.X,
        RelativeScale.Y * DeltaScale.Y,
        RelativeScale.Z * DeltaScale.Z);
    UpdateRelativeTransform();
}

// ──────────────────────────────
// World API
// ──────────────────────────────
FTransform USceneComponent::GetWorldTransform() const
{
    if (AttachParent)
        return AttachParent->GetWorldTransform() * RelativeTransform;
    return RelativeTransform;
}

void USceneComponent::SetWorldTransform(const FTransform& W)
{
    if (AttachParent)
    {
        const FTransform ParentWorld = AttachParent->GetWorldTransform();
        RelativeTransform = ParentWorld.Inverse() * W;
    }
    else
    {
        RelativeTransform = W;
    }

    RelativeLocation = RelativeTransform.Translation;
    RelativeRotation = RelativeTransform.Rotation;
    RelativeScale = RelativeTransform.Scale3D;
}
 
void USceneComponent::SetWorldLocation(const FVector& L)
{
    FTransform W = GetWorldTransform();
    W.Translation = L;
    SetWorldTransform(W);
}
FVector USceneComponent::GetWorldLocation() const
{
    return GetWorldTransform().Translation;
}

void USceneComponent::SetWorldRotation(const FQuat& R)
{
    FTransform W = GetWorldTransform();
    W.Rotation = R;
    SetWorldTransform(W);
}
FQuat USceneComponent::GetWorldRotation() const
{
    if (AttachParent != nullptr)
    {
        return RelativeTransform.Rotation * AttachParent->GetWorldRotation();
    }
    return RelativeTransform.Rotation;
}

void USceneComponent::SetWorldScale(const FVector& S)
{
    FTransform W = GetWorldTransform();
    W.Scale3D = S;
    SetWorldTransform(W);
}
FVector USceneComponent::GetWorldScale() const
{
    return GetWorldTransform().Scale3D;
}

void USceneComponent::AddWorldOffset(const FVector& Delta)
{
    FTransform W = GetWorldTransform();
    W.Translation = W.Translation + Delta;
    SetWorldTransform(W);
}

void USceneComponent::AddWorldRotation(const FQuat& DeltaRot)
{
    FTransform W = GetWorldTransform();
    W.Rotation = DeltaRot * W.Rotation;
    SetWorldTransform(W);
}


void USceneComponent::SetWorldLocationAndRotation(const FVector& L, const FQuat& R)
{
    FTransform W = GetWorldTransform();
    W.Translation = L;
    W.Rotation = R;
    SetWorldTransform(W);
}

void USceneComponent::AddLocalOffset(const FVector& Delta)
{
    const FVector parentDelta = RelativeRotation.RotateVector(Delta);
    RelativeLocation = RelativeLocation + parentDelta;
    UpdateRelativeTransform();
}

void USceneComponent::AddLocalRotation(const FQuat& DeltaRot)
{
    RelativeRotation = (RelativeRotation * DeltaRot).GetNormalized(); // 로컬: 우측곱
    UpdateRelativeTransform();
}

void USceneComponent::SetLocalLocationAndRotation(const FVector& L, const FQuat& R)
{
    RelativeLocation = L;
    RelativeRotation = R.GetNormalized();
    UpdateRelativeTransform();
}


FMatrix USceneComponent::GetWorldMatrix() const
{
    //TODO Cache 해놓으면 좋을 것 같다.
    return GetWorldTransform().ToMatrixWithScaleLocalXYZ();
}

// ──────────────────────────────
// Attach / Detach
// ──────────────────────────────
void USceneComponent::SetupAttachment(USceneComponent* InParent, EAttachmentRule Rule)
{
    if (AttachParent == InParent) return;

    const FTransform OldWorld = GetWorldTransform();

    // 기존 부모에서 제거
    if (AttachParent)
    {
        auto& Siblings = AttachParent->AttachChildren;
        Siblings.erase(std::remove(Siblings.begin(), Siblings.end(), this), Siblings.end());
    }

    // 새 부모 설정
    AttachParent = InParent;
    if (AttachParent)
        AttachParent->AttachChildren.push_back(this);

    // 규칙 적용
    if (AttachParent)
    {
        if (Rule == EAttachmentRule::KeepWorld)
        {
            const FTransform ParentWorld = AttachParent->GetWorldTransform();
            RelativeTransform = ParentWorld.Inverse() * OldWorld;
        }
        // KeepRelative: 기존 RelativeTransform 유지
    }
    else
    {
        if (Rule == EAttachmentRule::KeepWorld)
            RelativeTransform = OldWorld;
    }

    RelativeLocation = RelativeTransform.Translation;
    RelativeRotation = RelativeTransform.Rotation;
    RelativeScale = RelativeTransform.Scale3D;
}

void USceneComponent::DetachFromParent(bool bKeepWorld)
{
    const FTransform OldWorld = GetWorldTransform();

    if (AttachParent)
    {
        auto& Siblings = AttachParent->AttachChildren;
        Siblings.erase(std::remove(Siblings.begin(), Siblings.end(), this), Siblings.end());
        AttachParent = nullptr;
    }

    if (bKeepWorld)
        RelativeTransform = OldWorld;

    RelativeLocation = RelativeTransform.Translation;
    RelativeRotation = RelativeTransform.Rotation;
    RelativeScale = RelativeTransform.Scale3D;
}

// ──────────────────────────────
// 내부 유틸
// ──────────────────────────────
void USceneComponent::UpdateRelativeTransform()
{
    RelativeTransform = FTransform(RelativeLocation, RelativeRotation, RelativeScale);
}

// Duplicate function

void USceneComponent::CopyCommonProperties(USceneComponent* Target)
{
    if (!Target) return;

    // Transform 속성 복사 (모든 SceneComponent 공통)
    Target->RelativeLocation = this->RelativeLocation;
    Target->RelativeRotation = this->RelativeRotation;
    Target->RelativeScale = this->RelativeScale;
    Target->UpdateRelativeTransform();

    // 원본(this)의 자식 정보를 임시로 복사 (생성자가 초기화했으므로)
    // DuplicateSubObjects에서 이를 사용하여 자식들을 재귀 복제
    Target->AttachChildren = this->AttachChildren;
}

UObject* USceneComponent::Duplicate()
{
    // 올바른 타입으로 생성 (자식 클래스 포함)
    USceneComponent* DuplicatedComponent = Cast<USceneComponent>(NewObject(GetClass()));

    // 공통 속성 복사
    CopyCommonProperties(DuplicatedComponent);

    // 자식 클래스별 추가 속성 복사 및 계층 구조 복제
    DuplicatedComponent->DuplicateSubObjects();

    return DuplicatedComponent;
}
 
/**
 * @brief USceneComponent의 Internal 복사 함수
 * 원본이 들고 있던 Component를 각 Component의 복사함수를 호출하여 받아온 후 새로 담아서 처리함
 */
void USceneComponent::DuplicateSubObjects()
{
    Super_t::DuplicateSubObjects();

    // AttachChildren을 재귀적으로 복제
    TArray<USceneComponent*> DuplicatedChildren;
    for (USceneComponent* Component : AttachChildren)
    {
        USceneComponent* Child = Cast<USceneComponent>(Component->Duplicate());
        Child->AttachParent = this;

        UE_LOG("Child Name is %s", Child->GetName().c_str());

        DuplicatedChildren.push_back(Child);
    }
    AttachChildren = DuplicatedChildren;
}


UObject* USceneComponent::Duplicate(FObjectDuplicationParameters Parameters)
{
    auto DupObject = static_cast<USceneComponent*>(Super_t::Duplicate(Parameters));

    // 기본 인자 세팅  
    DupObject->RelativeLocation = RelativeLocation;
    DupObject->RelativeRotation = RelativeRotation;
    DupObject->RelativeScale = RelativeScale;
    DupObject->UpdateRelativeTransform();

    //계층 구조 
    if (AttachParent != nullptr)
    {
        if (auto It = Parameters.DuplicationSeed.find(AttachParent); It != Parameters.DuplicationSeed.end())
        { 
            DupObject->AttachParent = static_cast<USceneComponent*>(It->second); 
            DupObject->UpdateRelativeTransform();
        }
    } 

    for (auto& Child : AttachChildren)
    {
        if (auto It = Parameters.DuplicationSeed.find(Child); It != Parameters.DuplicationSeed.end())
        {
            DupObject->AttachChildren.Add(static_cast<USceneComponent*>(It->second));
        }
        else
        {
            auto Params = InitStaticDuplicateObjectParams(Child, DupObject->GetOuter(), FName::GetNone(), Parameters.DuplicationSeed, Parameters.CreatedObjects);
            auto DupComponent= static_cast<USceneComponent*>(Child->Duplicate(Params));
            DupObject->AttachChildren.Add(DupComponent);
        }
    }

    return DupObject;
}
