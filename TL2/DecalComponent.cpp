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
#include "CameraActor.h"
#include "World.h"
#include "VertexData.h"
#include "BVH.h"

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
    MaxAlpha = 1.0f;
    bIsOrthoMatrix = true;

    for (int i = 0; i < 4; ++i)
    {
        CurrentStateElapsedTime[i] = 0.0f;
    }
    DecalCurrentState = EDecalState::FadeIn;

    // Billboard setup (Editor only)
    auto& ResourceManager = UResourceManager::GetInstance();
    BillboardQuad = ResourceManager.Get<UTextQuad>("Billboard");
    BillboardTexture = ResourceManager.Load<UTexture>("Editor/Decal/DecalActor_64x.dds");
    BillboardMaterial = ResourceManager.Load<UMaterial>("Billboard.hlsl");

    SetTickEnabled(true);
}

UDecalComponent::~UDecalComponent()
{

}

void UDecalComponent::TickComponent(float DeltaSeconds)
{
    DecalAnimTick(DeltaSeconds);
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

// Find Affected Meshes
TArray<UStaticMeshComponent*> UDecalComponent::FindAffectedMeshes(UWorld* World)
{
    TArray<UStaticMeshComponent*> AffectedMeshes;

    if (!World)
        return AffectedMeshes;

    FBound DecalAABB = GetDecalBoundingBox();

    FOrientedBox DecalOBB = GetDecalOrientedBox();

    // 1단계: BVH를 사용한 Broad Phase 후보군 필터링
    TArray<AActor*> BroadPhaseCandidates;
    FBVH* BVH = World->GetBVH();

    if (BVH)
    {
        // BVH를 통해 Decal AABB와 교차하는 Actor들만 빠르게 찾기
        BVH->IntersectAABB(DecalAABB, BroadPhaseCandidates);
    }
    else
    {
        // BVH가 없는 경우 모든 Actor를 후보군으로 (Fallback)
        ULevel* Level = World->GetLevel();
        if (Level)
        {
            BroadPhaseCandidates = Level->GetActors();
        }
    }

    // Broad Phase 후보군에서 실제 영향받는 메시 찾기
    for (AActor* Actor : BroadPhaseCandidates)
    {
        if (!Actor || Actor->GetActorHiddenInGame())
            continue;

        // Actor의 모든 컴포넌트 검사
        const TSet<UActorComponent*>& Components = Actor->GetComponents();

        for (UActorComponent* Component : Components)
        {
            if (!Component)
                continue;

            // StaticMeshComponent인지 확인
            UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component);
            if (!StaticMeshComp)
                continue;

            // StaticMesh가 없으면 스킵
            if (!StaticMeshComp->GetStaticMesh())
                continue;

            // 2단계: AABB vs AABB (컴포넌트 레벨 필터링)
            FBound ComponentAABB = StaticMeshComp->GetWorldBoundingBox();
            if (!DecalAABB.IsIntersect(ComponentAABB))
                continue;

            // 3단계: OBB vs OBB (정밀 검사 - SAT)
            FOrientedBox ComponentOBB = StaticMeshComp->GetWorldOrientedBox();
            if (DecalOBB.Intersects(ComponentOBB))
            {
                AffectedMeshes.push_back(StaticMeshComp);
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

    // Editor 비주얼 (항상 렌더링)
    RenderEditorVisuals(Renderer, View, Proj);

    // 실제 Decal 투영 (SF_Decals 플래그에 따라 World.cpp에서 제어)
    RenderDecalProjection(Renderer, View, Proj);
}

void UDecalComponent::RenderEditorVisuals(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
    if (!Renderer)
    {
        return;
    }

    // Editor 모드에서만 실행
    if (GWorld && GWorld->WorldType == EWorldType::Editor)
    {
        // 1. Owner가 선택된 경우에만 OBB Drawing
        RenderOBB(Renderer);

        // 2. Billboard rendering
        RenderBillboard(Renderer, View, Proj);
    }
}

void UDecalComponent::RenderDecalProjection(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
    if (!Renderer || !DecalTexture)
    {
        return;
    }

    // Affected Meshes 찾기
    TArray<UStaticMeshComponent*> AffectedMeshes = FindAffectedMeshes(GWorld);
    if (AffectedMeshes.empty())
        return;

    URenderingStatsCollector& StatsCollector = URenderingStatsCollector::GetInstance();
    FDecalRenderingStats& DecalStats = StatsCollector.GetDecalStats();

    DecalStats.ActiveDecalCount++;
    DecalStats.AffectedMeshesCount += static_cast<uint32>(AffectedMeshes.size());

    // Decal Shader 및 파이프라인 준비
    UShader* DecalProjShader = UResourceManager::GetInstance().Load<UShader>("ProjectionDecal.hlsl");

    Renderer->PrepareShader(DecalProjShader);
    Renderer->OMSetBlendState(true);
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);
     
    // reset viewfrustum posision 
   
    FMatrix DecalView;
    FMatrix DecalProj;

    //Orthographic Matrix
    if(bIsOrthoMatrix)
    {
        FTransform DecalXform = GetWorldTransform();  
        DecalView = DecalXform.ToMatrixWithScaleLocalXYZ().InverseAffine();

        FVector Scale = GetWorldScale();
        const float OrthoWidth = Scale.Y;
        const float OrthoHeight = Scale.Z;
        const float NearZ = -0.5f * Scale.X;
        const float FarZ = 0.5f * Scale.X;
        DecalProj = FMatrix::OrthoLH(OrthoWidth, OrthoHeight, NearZ, FarZ);
    }

    //Perspective Matrix
    else
    {
        FTransform DecalXform = GetWorldTransform();
        DecalXform.Scale3D = FVector(1.0f, 1.0f, 1.0f);
        FVector Scale = GetWorldScale();
        {
            const FVector projForward = DecalXform.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
            DecalXform.Translation = DecalXform.Translation + projForward * (-0.5f * Scale.X);
        }
        DecalView = DecalXform.ToMatrixWithScaleLocalXYZ().InverseAffine();

        const float FarX = FMath::Max(Scale.X, 1e-3f);
        const float NearX = 0.01f;

        const float Aspect = Scale.Y / Scale.Z;
         
        float tanHalfFov = Scale.Z / (2.0f * FarX);
        const float FovRad = 2.0f * atanf(tanHalfFov);

        DecalProj = FMatrix::PerspectiveFovLH(FovRad, Aspect, NearX, FarX);
    }

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
        const float Duration = GetFadeInDuration();
         
        if (Duration == 0.0f)
        {
            CurrentAlpha = MaxAlpha;
            DecalCurrentState = EDecalState::Delay;
            CurrentStateElapsedTime[static_cast<uint8>(EDecalState::Delay)] = 0.0f;
        }
        else
        {
            CurrentAlpha = FMath::Clamp(CurrentStateElapsedTime[static_cast<uint8>(EDecalState::FadeIn)] / Duration, 0.0f, MaxAlpha);
            if (CurrentAlpha == MaxAlpha)
            {
                DecalCurrentState = EDecalState::Delay;
                CurrentStateElapsedTime[static_cast<uint8>(EDecalState::Delay)] = 0.0f; 
            }

        }
         
        break;
    }
    case EDecalState::Delay:
    {
        CurrentAlpha = MaxAlpha;
        if (FadeStartDelay == 0.0f)
        {

        }
        else if (CurrentStateElapsedTime[static_cast<uint8>(EDecalState::Delay)] > GetFadeStartDelay())
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

void UDecalComponent::CreateBillboardVertices()
{
    TArray<FBillboardVertexInfo_GPU> vertices;

    // 단일 쿼드의 4개 정점 생성 (카메라를 향하는 평면)
    float halfW = BillboardWidth * 0.5f;
    float halfH = BillboardHeight * 0.5f;

    FBillboardVertexInfo_GPU Info;

    // UV 좌표 (전체 텍스처 사용)
    float UCoord = 0.0f;
    float VCoord = 0.0f;
    float ULength = 1.0f;
    float VLength = 1.0f;

    // 정점 0: 좌상단
    Info.Position[0] = -halfW;
    Info.Position[1] = halfH;
    Info.Position[2] = 0.0f;
    Info.CharSize[0] = BillboardWidth;
    Info.CharSize[1] = BillboardHeight;
    Info.UVRect[0] = UCoord;
    Info.UVRect[1] = VCoord;
    Info.UVRect[2] = ULength;
    Info.UVRect[3] = VLength;
    vertices.push_back(Info);

    // 정점 1: 우상단
    Info.Position[0] = halfW;
    Info.Position[1] = halfH;
    Info.Position[2] = 0.0f;
    vertices.push_back(Info);

    // 정점 2: 좌하단
    Info.Position[0] = -halfW;
    Info.Position[1] = -halfH;
    Info.Position[2] = 0.0f;
    vertices.push_back(Info);

    // 정점 3: 우하단
    Info.Position[0] = halfW;
    Info.Position[1] = -halfH;
    Info.Position[2] = 0.0f;
    vertices.push_back(Info);

    // 동적 버텍스 버퍼 업데이트
    UResourceManager::GetInstance().UpdateDynamicVertexBuffer("Billboard", vertices);
}

void UDecalComponent::RenderBillboard(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
    if (!BillboardMaterial || !BillboardTexture || !BillboardQuad)
        return;

    // 카메라 정보 가져오기
    UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
    if (!World)
        return;

    ACameraActor* CameraActor = World->GetCameraActor();
    if (!CameraActor)
        return;

    FVector CamRight = CameraActor->GetActorRight();
    FVector CamUp = CameraActor->GetActorUp();

    // 빌보드 위치 설정 (데칼 액터의 위치)
    FVector BillboardPos = GetWorldLocation();

    // 텍스처 로드 
    BillboardMaterial->Load("Editor/Decal/DecalActor_64x.dds", Renderer->GetRHIDevice()->GetDevice());

    // 상수 버퍼 업데이트
    Renderer->UpdateBillboardConstantBuffers(BillboardPos, View, Proj, CamRight, CamUp);

    // 셰이더 준비
    UShader* CompShader = BillboardMaterial->GetShader();
    Renderer->PrepareShader(CompShader);

    ID3D11DeviceContext* DeviceContext = Renderer->GetRHIDevice()->GetDeviceContext();

    // InputLayout 설정 (중요!)
    DeviceContext->IASetInputLayout(CompShader->GetInputLayout());

    // 빌보드 정점 생성 및 버퍼 업데이트
    CreateBillboardVertices();

    // 렌더링
    Renderer->OMSetBlendState(true);
    Renderer->RSSetState(EViewModeIndex::VMI_Unlit);

    // 버퍼 및 텍스처 바인딩
    UINT Stride = sizeof(FBillboardVertexInfo_GPU);
    UINT Offset = 0;
    ID3D11Buffer* VertexBuffer = BillboardQuad->GetVertexBuffer();
    ID3D11Buffer* IndexBuffer = BillboardQuad->GetIndexBuffer();

    DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // 텍스처 바인딩 (중요!)
    UTexture* CompTexture = BillboardMaterial->GetTexture();
    if (CompTexture && CompTexture->GetShaderResourceView())
    {
        ID3D11ShaderResourceView* TextureSRV = CompTexture->GetShaderResourceView();
        Renderer->GetRHIDevice()->PSSetDefaultSampler(0);
        DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);
    }

    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->DrawIndexed(BillboardQuad->GetIndexCount(), 0, 0);

    Renderer->OMSetBlendState(false);
}

void UDecalComponent::RenderOBB(URenderer* Renderer)
{
    if (!Renderer || !Owner)
    {
        return;
    }

    // Owner가 선택된 경우에만 OBB Drawing
    if (!USelectionManager::GetInstance().IsActorSelected(Owner))
    {
        return;
    }

    FOrientedBox OBB = GetDecalOrientedBox();
    TArray<FVector> Corners = OBB.GetCorners();
	const FVector4 LineColor(0.0f, 1.0f, 0.0f, 1.0f); // 녹색

    if (Corners.size() == 8)
    {
        // Bottom face
        Renderer->AddLine(Corners[0], Corners[1], LineColor);
        Renderer->AddLine(Corners[1], Corners[3], LineColor);
        Renderer->AddLine(Corners[3], Corners[2], LineColor);
        Renderer->AddLine(Corners[2], Corners[0], LineColor);

        // Top face
        Renderer->AddLine(Corners[4], Corners[5], LineColor);
        Renderer->AddLine(Corners[5], Corners[7], LineColor);
        Renderer->AddLine(Corners[7], Corners[6], LineColor);
        Renderer->AddLine(Corners[6], Corners[4], LineColor);

        // Vertical edges
        Renderer->AddLine(Corners[0], Corners[4], LineColor);
        Renderer->AddLine(Corners[1], Corners[5], LineColor);
        Renderer->AddLine(Corners[2], Corners[6], LineColor);
        Renderer->AddLine(Corners[3], Corners[7], LineColor);
    }
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
    DuplicatedComponent->bIsOrthoMatrix = bIsOrthoMatrix;
    
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
    DupObject->MaxAlpha = MaxAlpha;

    DupObject->bIsOrthoMatrix = bIsOrthoMatrix;
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
