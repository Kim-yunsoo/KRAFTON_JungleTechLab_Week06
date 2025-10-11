// DecalComponent.cpp
#include "pch.h"
#include "DecalComponent.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "SViewportWindow.h"
#include "D3D11RHI.h"
#include "Actor.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "AABoundingBoxComponent.h"
#include "RenderingStats.h"
#include "SelectionManager.h"

UDecalComponent::UDecalComponent()
{
    // 기본 Decal 텍스처 로드
    SetDecalTexture("Editor/Decal/DefaultDecalTexture.jpg");

    //Decal Stat  Init
    SortOrder = 0;
    FadeInDuration = 3;
    FadeStartDelay = 3;
    FadeDuration = 3; 

    CurrentAlpha = 1.0f;
    for (int i = 0; i < 4; ++i)
    {
        CurrentStateElapsedTime[i] = 0.0f;
    }
    DecalCurrentState = EDecalState::FadeIn;
}

UDecalComponent::~UDecalComponent()
{

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
void UDecalComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
    if (!Renderer)
    {
        return;
    }

    // 1. Owner가 선택된 경우에만 OBB Drawing
    if (Owner && USelectionManager::GetInstance().IsActorSelected(Owner))
    {
        FOrientedBox OBB = GetDecalOrientedBox();
        TArray<FVector> Corners = OBB.GetCorners();
        const FVector4 Yellow(1.0f, 1.0f, 0.0f, 1.0f);

        if (Corners.size() == 8)
        {
            // Bottom face
            Renderer->AddLine(Corners[0], Corners[1], Yellow);
            Renderer->AddLine(Corners[1], Corners[3], Yellow);
            Renderer->AddLine(Corners[3], Corners[2], Yellow);
            Renderer->AddLine(Corners[2], Corners[0], Yellow);

            // Top face
            Renderer->AddLine(Corners[4], Corners[5], Yellow);
            Renderer->AddLine(Corners[5], Corners[7], Yellow);
            Renderer->AddLine(Corners[7], Corners[6], Yellow);
            Renderer->AddLine(Corners[6], Corners[4], Yellow);

            // Vertical edges
            Renderer->AddLine(Corners[0], Corners[4], Yellow);
            Renderer->AddLine(Corners[1], Corners[5], Yellow);
            Renderer->AddLine(Corners[2], Corners[6], Yellow);
            Renderer->AddLine(Corners[3], Corners[7], Yellow);
        }
    }

    if (!DecalTexture)
    {
        return;
    }

    // 2. Affected Meshes 찾기
    TArray<UStaticMeshComponent*> AffectedMeshes = FindAffectedMeshes(GWorld);
    if (AffectedMeshes.empty())
        return;

    URenderingStatsCollector& StatsCollector = URenderingStatsCollector::GetInstance();
    FDecalRenderingStats& DecalStats = StatsCollector.GetDecalStats();

	DecalStats.ActiveDecalCount++;
    DecalStats.AffectedMeshesCount += static_cast<uint32>(AffectedMeshes.size());

    // 3. Decal Shader 및 파이프라인 준비
    UShader* DecalProjShader = UResourceManager::GetInstance().Load<UShader>("ProjectionDecal.hlsl");

    Renderer->PrepareShader(DecalProjShader);
    Renderer->OMSetBlendState(true);
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);

    // Decal View/Proj from this component
    FMatrix DecalView = GetWorldTransform().ToMatrixWithScaleLocalXYZ().InverseAffine();
    FVector Scale = GetRelativeScale();

    const float OrthoWidth = Scale.Y;
    const float OrthoHeight = Scale.Z;
    const float NearZ = -0.5f * Scale.X;
    const float FarZ = 0.5f * Scale.X;

    FMatrix DecalProj = FMatrix::OrthoLH(OrthoWidth, OrthoHeight, NearZ, FarZ);

    ID3D11DeviceContext* DevieContext = Renderer->GetRHIDevice()->GetDeviceContext();

    // Bind decal texture (t0)
    UTexture* DecalTexture = GetDecalTexture();
    if (DecalTexture && DecalTexture->GetShaderResourceView())
    {
        ID3D11ShaderResourceView* TextureSRV = DecalTexture->GetShaderResourceView();
        DevieContext->PSSetShaderResources(0, 1, &TextureSRV);
        Renderer->GetRHIDevice()->PSSetDefaultSampler(0);
    }

    for (UStaticMeshComponent* StaticMeshComponent : AffectedMeshes)
    {
        UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
        if (!StaticMeshComponent || !Mesh)
            continue;

        // Per-mesh constant buffers
        // Reuse b4 to carry decal view/proj
        Renderer->UpdateConstantBuffer(StaticMeshComponent->GetWorldMatrix(), View, Proj);
        Renderer->UpdateInvWorldBuffer(DecalView, DecalProj);
        Renderer->UpdateColorBuffer(FVector4{ 1.0f, 1.0f, 1.0f, CurrentAlpha });

        UINT Stride = 0;
        switch (Mesh->GetVertexType())
        {
        case EVertexLayoutType::PositionColor:
            Stride = sizeof(FVertexSimple);
            break;
        case EVertexLayoutType::PositionColorTexturNormal:
            Stride = sizeof(FVertexDynamic);
            break;
        case EVertexLayoutType::PositionBillBoard:
            Stride = sizeof(FBillboardVertexInfo_GPU);
            break;
        default:
            continue;
        }

        UINT Offset = 0;
        ID3D11Buffer* VertexBuffer = Mesh->GetVertexBuffer();
        ID3D11Buffer* IndexBuffer = Mesh->GetIndexBuffer();

        DevieContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
        DevieContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        DevieContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        DevieContext->DrawIndexed(Mesh->GetIndexCount(), 0, 0);

		// Decal 드로우콜 통계 증가
        StatsCollector.IncrementDecalDrawCalls();
    }

    // Unbind SRVs
    ID3D11ShaderResourceView* NullSRV[2] = { nullptr, nullptr };
    DevieContext->PSSetShaderResources(0, 2, NullSRV);

    Renderer->OMSetBlendState(false);
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual);
}

void UDecalComponent::SetDecalTexture(const FString& TexturePath)
{
    UResourceManager& ResourceManager = UResourceManager::GetInstance();
    DecalTexture = ResourceManager.Load<UTexture>(TexturePath);
}

void UDecalComponent::DecalAnimTick(float DeltaTime)
{ 
    const uint8 stateIndex = static_cast<uint8>(DecalCurrentState);

    if (DecalCurrentState != EDecalState::Finished)
    {
        if (stateIndex < (uint8)4)
        {
            CurrentStateElapsedTime[stateIndex] += DeltaTime;
        }
    }

    ActivateFadeEffect();
}

void UDecalComponent::ActivateFadeEffect()
{
    switch (DecalCurrentState)
    {
    case EDecalState::FadeIn:
    {
        const float Duration = FMath::Max(0.0f, GetFadeInDuration());
        if (Duration == 0.0f)
        {
            CurrentAlpha = 1.0f;
            DecalCurrentState = EDecalState::Delay;
            CurrentStateElapsedTime[static_cast<uint8>(EDecalState::Delay)] = 0.0f;
        }
        else
        {
            CurrentAlpha = FMath::Clamp(CurrentStateElapsedTime[static_cast<uint8>(EDecalState::FadeIn)] / Duration, 0.0f, 1.0f);
            if (CurrentAlpha == 1.0f)
            {
                DecalCurrentState = EDecalState::Delay;
                CurrentStateElapsedTime[static_cast<uint8>(EDecalState::Delay)] = 0.0f; 
            }

        }
         
        break;
    }
    case EDecalState::Delay:
    {
        CurrentAlpha = 1.0f;
        if (CurrentStateElapsedTime[static_cast<uint8>(EDecalState::Delay)] > GetFadeStartDelay())
        {
            DecalCurrentState = EDecalState::FadingOut;
            CurrentStateElapsedTime[static_cast<uint8>(EDecalState::FadingOut)] = 0.0f;
        }
        break;
    }

    case EDecalState::FadingOut:
    {
        const float Duration = FMath::Max(0.0f, GetFadeDuration());
        if (Duration == 0.0f)
        {
            CurrentAlpha = 0.0f;
            DecalCurrentState = EDecalState::Finished;
        }
        else
        {
            const float InvAlpha = FMath::Clamp(CurrentStateElapsedTime[static_cast<uint8>(EDecalState::FadingOut)] / Duration, 0.0f, 1.0f);
            CurrentAlpha = 1.0f - InvAlpha;
           if (InvAlpha >= 1.0f)
            {
                DecalCurrentState = EDecalState::Finished;  
            }
        }
        break;
    }
    case EDecalState::Finished:
    {
        CurrentAlpha = 0.0f; 
        break;
    }
    }
}

void UDecalComponent::StartFade()
{
    // Reset Timers
    for (int i = 0; i < 4; ++i)
    {
        CurrentStateElapsedTime[i] = 0.0f;
    }
    CurrentAlpha = 0.0f;
    DecalCurrentState = EDecalState::FadeIn;
}

UObject* UDecalComponent::Duplicate()
{
    UDecalComponent* DuplicatedComponent = Cast<UDecalComponent>(NewObject(GetClass()));

    // 공통 속성 복사 (Transform, AttachChildren)
    CopyCommonProperties(DuplicatedComponent);
    
    // DecalComponent 전용 속성 복사
    DuplicatedComponent->DecalTexture = DecalTexture;
    DuplicatedComponent->DecalSize = DecalSize;
    DuplicatedComponent->SortOrder = SortOrder;

    DuplicatedComponent->FadeInDuration = FadeInDuration;
    DuplicatedComponent->FadeStartDelay = FadeStartDelay;
    DuplicatedComponent->FadeDuration = FadeDuration;
    DuplicatedComponent->CurrentAlpha = 0.0f;
    DuplicatedComponent->DecalCurrentState = EDecalState::FadeIn;

    for (int i = 0; i < 4; ++i)
    {
        DuplicatedComponent->CurrentStateElapsedTime[i] = 0.0f;
    }

    DuplicatedComponent->DuplicateSubObjects();

    return DuplicatedComponent;
}

UObject* UDecalComponent::Duplicate(FObjectDuplicationParameters Parameters)
{ 
    auto DupObject = static_cast<UDecalComponent*>(Super_t::Duplicate(Parameters));

    //얕복
    DupObject->DecalTexture = DecalTexture;
    DupObject->DecalSize = DecalSize;
    DupObject->SortOrder = SortOrder;

    DupObject->FadeInDuration = FadeInDuration;
    DupObject->FadeStartDelay = FadeStartDelay;
    DupObject->FadeDuration = FadeDuration;
     
    DupObject->CurrentAlpha = 0.0f;
    DupObject->DecalCurrentState = EDecalState::FadeIn;
    
    //시작은 0에서  시작 
    for (int i = 0; i < 4; ++i)
    {
        DupObject->CurrentStateElapsedTime[i] = 0.0f;
    }


    return DupObject;
}

void UDecalComponent::DuplicateSubObjects()
{
    // 부모의 깊은 복사 수행 (AttachChildren 재귀 복제)
    Super_t::DuplicateSubObjects();

    //TODO: Decal Component 복제해야하지  않나?
}
