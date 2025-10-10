#include "pch.h"
#include "DecalComponent.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "World.h"
#include "Level.h"
#include "Actor.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "AABoundingBoxComponent.h"

UDecalComponent::UDecalComponent()
{
    // Decal은 기본적으로 Material이 필요 없음 (전용 Shader 사용)
    // 기본 Decal 텍스처 로드
    SetDecalTexture("Data/DefaultDecalTexture.dds");
}

UDecalComponent::~UDecalComponent()
{
}

// Texture
void UDecalComponent::SetDecalTexture(const FString& TexturePath)
{
    auto& ResourceManager = UResourceManager::GetInstance();
    DecalTexture = ResourceManager.Load<UTexture>(TexturePath);

    if (!DecalTexture)
    {
        UE_LOG("DecalComponent: Failed to load texture: %s", TexturePath.c_str());
    }
}

// Fade System
void UDecalComponent::StartFadeIn(float Duration)
{
    FadeState = EDecalFadeState::FadingIn;
    FadeDuration = Duration;
    FadeCurrentTime = 0.0f;
    FadeStartOpacity = GetCurrentOpacity();
    FadeTargetOpacity = BaseOpacity;
}

void UDecalComponent::StartFadeOut(float Duration)
{
    FadeState = EDecalFadeState::FadingOut;
    FadeDuration = Duration;
    FadeCurrentTime = 0.0f;
    FadeStartOpacity = GetCurrentOpacity();
    FadeTargetOpacity = 0.0f;
}

void UDecalComponent::UpdateFade(float DeltaTime)
{
    if (FadeState == EDecalFadeState::None)
        return;

    FadeCurrentTime += DeltaTime;

    if (FadeCurrentTime >= FadeDuration)
    {
        // Fade 완료
        FadeCurrentTime = FadeDuration;
        FadeState = EDecalFadeState::None;
    }
}

float UDecalComponent::GetCurrentOpacity() const
{
    if (FadeState == EDecalFadeState::None)
        return BaseOpacity;

    // Linear interpolation
    float t = FadeCurrentTime / FadeDuration;
    t = FMath::Clamp(t, 0.0f, 1.0f);

    return FMath::Lerp(FadeStartOpacity, FadeTargetOpacity, t);
}

// Transform Matrices
FMatrix UDecalComponent::GetDecalToWorldMatrix() const
{
    return GetWorldMatrix();
}

FMatrix UDecalComponent::GetWorldToDecalMatrix() const
{
    return GetWorldMatrix().Inverse();
}

FBound UDecalComponent::GetDecalBoundingBox() const
{
    // Decal Box의 AABB 계산
    FVector WorldLocation = GetWorldLocation();
    FQuat WorldRotation = GetWorldRotation();

    // Decal 로컬 박스: [-Size/2, +Size/2]
    FVector HalfSize = (DecalSize * GetWorldScale()) * 0.5f;

    // 8개 모서리 점을 월드로 변환
    FVector Corners[8] = {
        FVector(-HalfSize.X, -HalfSize.Y, -HalfSize.Z),
        FVector(+HalfSize.X, -HalfSize.Y, -HalfSize.Z),
        FVector(-HalfSize.X, +HalfSize.Y, -HalfSize.Z),
        FVector(+HalfSize.X, +HalfSize.Y, -HalfSize.Z),
        FVector(-HalfSize.X, -HalfSize.Y, +HalfSize.Z),
        FVector(+HalfSize.X, -HalfSize.Y, +HalfSize.Z),
        FVector(-HalfSize.X, +HalfSize.Y, +HalfSize.Z),
        FVector(+HalfSize.X, +HalfSize.Y, +HalfSize.Z),
    };

    FVector MinCorner = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxCorner = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int i = 0; i < 8; ++i)
    {
        // 로컬 → 월드 변환
        FVector WorldCorner = WorldRotation.RotateVector(Corners[i]) + WorldLocation;

        MinCorner.X = FMath::Min(MinCorner.X, WorldCorner.X);
        MinCorner.Y = FMath::Min(MinCorner.Y, WorldCorner.Y);
        MinCorner.Z = FMath::Min(MinCorner.Z, WorldCorner.Z);

        MaxCorner.X = FMath::Max(MaxCorner.X, WorldCorner.X);
        MaxCorner.Y = FMath::Max(MaxCorner.Y, WorldCorner.Y);
        MaxCorner.Z = FMath::Max(MaxCorner.Z, WorldCorner.Z);
    }

    return FBound(MinCorner, MaxCorner);
}

FOrientedBox UDecalComponent::GetDecalOrientedBox() const
{
    return FOrientedBox(
        GetWorldLocation(),
        (DecalSize * GetWorldScale()) * 0.5f,
        GetWorldRotation()
    );
}

FOrientedBox UDecalComponent::GetActorOrientedBox(AStaticMeshActor* Actor) const
{
    if (!Actor)
        return FOrientedBox();

    UAABoundingBoxComponent* CollisionComp = Actor->CollisionComponent;
    if (!CollisionComp)
        return FOrientedBox();

    FBound AABB = CollisionComp->GetWorldBoundFromCube();
    FVector Center = (AABB.Max + AABB.Min) * 0.5f;
    FVector HalfExtents = (AABB.Max - AABB.Min) * 0.5f;

    return FOrientedBox(Center, HalfExtents, Actor->GetActorRotation());
}

// Find Affected Meshes
TArray<UStaticMeshComponent*> UDecalComponent::FindAffectedMeshes(UWorld* World)
{
    TArray<UStaticMeshComponent*> AffectedMeshes;

    if (!World)
        return AffectedMeshes;

    // 1단계: Decal AABB 계산 (빠른 필터링용)
    FBound DecalAABB = GetDecalBoundingBox();

    // 2단계: Decal OBB 계산 (정밀 검사용)
    FOrientedBox DecalOBB = GetDecalOrientedBox();

    // World의 모든 Actor 순회
    ULevel* Level = World->GetLevel();
    if (!Level)
        return AffectedMeshes;

    const TArray<AActor*>& Actors = Level->GetActors();

    for (AActor* Actor : Actors)
    {
        if (!Actor || Actor->GetActorHiddenInGame())
            continue;

        // StaticMeshActor만 검사
        AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);
        if (!StaticMeshActor)
            continue;

        // Collision Component로 충돌 검사
        UAABoundingBoxComponent* CollisionComp = StaticMeshActor->CollisionComponent;
        if (!CollisionComp)
            continue;

        FBound ActorAABB = CollisionComp->GetWorldBoundFromCube();

        // 1단계: AABB vs AABB (빠른 필터링)
        if (!DecalAABB.IsIntersect(ActorAABB))
            continue;

        // 2단계: OBB vs OBB (정밀 검사 - SAT)
        FOrientedBox ActorOBB = GetActorOrientedBox(StaticMeshActor);
        if (DecalOBB.Intersects(ActorOBB))
        {
            UStaticMeshComponent* MeshComp = StaticMeshActor->GetStaticMeshComponent();
            if (MeshComp)
            {
                AffectedMeshes.push_back(MeshComp);
            }
        }
    }

    return AffectedMeshes;
}

// Rendering
void UDecalComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, FViewport* Viewport)
{
    if (!DecalTexture || !Renderer)
        return;

    // Renderer를 통해 Decal 렌더링
    Renderer->RenderDecalComponent(this, View, Proj, Viewport);

    // --- OBB Drawing ---
    FOrientedBox OBB = GetDecalOrientedBox();
    TArray<FVector> Corners = OBB.GetCorners();
    const FVector4 Yellow(1.0f, 1.0f, 0.0f, 1.0f);

    if (Corners.size() == 8)
    {
        // Draw bottom face
        Renderer->AddLine(Corners[0], Corners[1], Yellow);
        Renderer->AddLine(Corners[1], Corners[3], Yellow);
        Renderer->AddLine(Corners[3], Corners[2], Yellow);
        Renderer->AddLine(Corners[2], Corners[0], Yellow);

        // Draw top face
        Renderer->AddLine(Corners[4], Corners[5], Yellow);
        Renderer->AddLine(Corners[5], Corners[7], Yellow);
        Renderer->AddLine(Corners[7], Corners[6], Yellow);
        Renderer->AddLine(Corners[6], Corners[4], Yellow);

        // Draw vertical edges
        Renderer->AddLine(Corners[0], Corners[4], Yellow);
        Renderer->AddLine(Corners[1], Corners[5], Yellow);
        Renderer->AddLine(Corners[2], Corners[6], Yellow);
        Renderer->AddLine(Corners[3], Corners[7], Yellow);
    }
}

// Duplicate Support
UObject* UDecalComponent::Duplicate()
{
    UDecalComponent* DuplicatedComponent = Cast<UDecalComponent>(NewObject(GetClass()));

    // 공통 속성 복사 (Transform, AttachChildren)
    CopyCommonProperties(DuplicatedComponent);

    // DecalComponent 전용 속성 복사
    DuplicatedComponent->DecalTexture = this->DecalTexture;
    DuplicatedComponent->DecalSize = this->DecalSize;
    DuplicatedComponent->BaseOpacity = this->BaseOpacity;
    DuplicatedComponent->bProjectOnBackfaces = this->bProjectOnBackfaces;

    // Fade 상태는 복사하지 않음 (새로운 인스턴스는 Fade 없이 시작)
    DuplicatedComponent->FadeState = EDecalFadeState::None;
    DuplicatedComponent->FadeDuration = this->FadeDuration;
    DuplicatedComponent->FadeCurrentTime = 0.0f;
    DuplicatedComponent->FadeStartOpacity = 0.0f;
    DuplicatedComponent->FadeTargetOpacity = this->BaseOpacity;

    DuplicatedComponent->DuplicateSubObjects();
    return DuplicatedComponent;
}

void UDecalComponent::DuplicateSubObjects()
{
    // 부모의 깊은 복사 수행 (AttachChildren 재귀 복제)
    Super_t::DuplicateSubObjects();
}
