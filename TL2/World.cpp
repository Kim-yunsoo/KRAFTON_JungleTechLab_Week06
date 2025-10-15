#include "pch.h"
#include "SelectionManager.h"
#include "Picking.h"
#include "SceneLoader.h"
#include "CameraActor.h"
#include "StaticMeshActor.h"
#include "CameraComponent.h"
#include "ObjectFactory.h"
#include "TextRenderComponent.h"
#include "AABoundingBoxComponent.h"
#include "FViewport.h"
#include "SViewportWindow.h"
#include "SMultiViewportWindow.h"
#include "StaticMesh.h"
#include "ObjManager.h"
#include "SceneRotationUtils.h"
#include "Frustum.h"
#include "Octree.h"
#include "BVH.h"
#include "UEContainer.h"
#include "DecalComponent.h"
#include "DecalActor.h"
#include "BillboardComponent.h"
#include "RenderingStats.h"
#include "MovementComponent.h"
#include "RotatingMovementComponent.h"
#include "ProjectileMovementComponent.h"
#include "LightComponent.h"
#include "PointLightComponent.h"
#include "D3D11RHI.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

static inline FString GetBaseNameNoExt(const FString& Path)
{
    const size_t sep = Path.find_last_of("/\\");
    const size_t start = (sep == FString::npos) ? 0 : sep + 1;

    const FString ext = ".obj";
    size_t end = Path.size();
    if (end >= ext.size() && Path.compare(end - ext.size(), ext.size(), ext) == 0)
    {
        end -= ext.size();
    }
    if (start <= end) return Path.substr(start, end - start);
    return Path;
}

static inline FVector2D WorldToScreen01(const FVector& P, const FMatrix& View, const FMatrix& Proj)
{
    FVector4 clip =  FVector4(P.X, P.Y, P.Z , 1.0f) * View * Proj; // (row-major 행 연산 흐름 유지)
    float w = FMath::Max((float)1e-6, (float)clip.W); // CORRECTED: Should be W
    FVector2D ndc = FVector2D(clip.X / w, clip.Y / w); // [-1,1]
    return FVector2D(ndc.X * 0.5f + 0.5f, -ndc.Y * 0.5f + 0.5f); // (0,0) 좌상 → (1,1) 우하
}

static float WorldRadiusToPixelRadius_Persp(
    float WorldRadius,
    float ViewZ,
    float FovYRadians,
    int   ScreenHeightPx)
{
    const float fy = 1.0f / std::tan(FovYRadians * 0.5f);
    return WorldRadius * (float(ScreenHeightPx) * 0.5f)  / FMath::Max(ViewZ, 1e-3f) * fy;
}

static inline bool WorldToScreenOutViewZ(
    const FVector& P,
    const FMatrix& View,
    const FMatrix& Proj,
    FVector2D& OutUV,
    float& OutViewZ)
{
    const FVector4 ViewSpace= FVector4(P.X, P.Y, P.Z, 1.0f) * View;
    OutViewZ = ViewSpace.Z;
    const FVector4 ClipSpace = ViewSpace * Proj;

    if (ClipSpace.W <= 1e-6f) return false;

    const float PerspDiv= 1.0f / ClipSpace.W;
    const FVector2D ndc(ClipSpace.X * PerspDiv, ClipSpace.Y * PerspDiv); // [-1,1] 
    OutUV = FVector2D(ndc.X * 0.5f + 0.5f, -ndc.Y * 0.5f + 0.5f);
    return true;
}
 



UWorld::UWorld() : ResourceManager(UResourceManager::GetInstance())
                   , UIManager(UUIManager::GetInstance())
                   , InputManager(UInputManager::GetInstance())
                   , SelectionManager(USelectionManager::GetInstance())
                   , BVH(nullptr)
{
    Level = NewObject<ULevel>();
}

UWorld::~UWorld()
{
    // Level의 Actors 정리 (PIE는 복제된 액터들만 삭제)
    if (Level)
    {
        for (AActor* Actor : Level->GetActors())
        {
            ObjectFactory::DeleteObject(Actor);
        }

        // Level 자체 정리
        ObjectFactory::DeleteObject(Level);
        Level = nullptr;
    }

    // PIE 월드가 아닐 때만 공유 리소스 삭제
    if (WorldType == EWorldType::Editor)
    {
        // 카메라 정리
        ObjectFactory::DeleteObject(MainCameraActor);
        MainCameraActor = nullptr;

        // Grid 정리
        ObjectFactory::DeleteObject(GridActor);
        GridActor = nullptr;

        // GizmoActor 정리
        ObjectFactory::DeleteObject(GizmoActor);
        GizmoActor = nullptr;

        // BVH 정리
        if (BVH)
        {
            delete BVH;
            BVH = nullptr;
        }

        // ObjManager 정리
        FObjManager::Clear();
    }
    else if (WorldType == EWorldType::PIE)
    {
        // PIE 월드의 BVH 정리 (PIE 전용으로 새로 생성했으므로 삭제 필요)
        if (BVH)
        {
            delete BVH;
            BVH = nullptr;
        }

        // PIE 월드는 공유 포인터만 nullptr로 설정 (삭제하지 않음)
        MainCameraActor = nullptr;
        GridActor = nullptr;
        GizmoActor = nullptr;
        Renderer = nullptr;
        MainViewport = nullptr;
        MultiViewport = nullptr;
    }
}

static void DebugRTTI_UObject(UObject* Obj, const char* Title)
{
    if (!Obj)
    {
        UE_LOG("[RTTI] Obj == null\r\n");
        return;
    }

    char buf[256];
    UE_LOG("========== RTTI CHECK ==========\r\n");
    if (Title)
    {
        std::snprintf(buf, sizeof(buf), "[RTTI] %s\r\n", Title);
        UE_LOG(buf);
    }

    // 1) 현재 동적 타입 이름
    std::snprintf(buf, sizeof(buf), "[RTTI] TypeName = %s\r\n", Obj->GetClass()->Name);
    UE_LOG(buf);

    // 2) IsA 체크 (파생 포함)
    std::snprintf(buf, sizeof(buf), "[RTTI] IsA<AActor>      = %d\r\n", (int)Obj->IsA<AActor>());
    UE_LOG(buf);
    std::snprintf(buf, sizeof(buf), "[RTTI] IsA<ACameraActor> = %d\r\n",
                  (int)Obj->IsA<ACameraActor>());
    UE_LOG(buf);

    //// 3) 정확한 타입 비교 (파생 제외)
    //std::snprintf(buf, sizeof(buf), "[RTTI] EXACT ACameraActor = %d\r\n",
    //    (int)(Obj->GetClass() == ACameraActor::StaticClass()));
    //UE_LOG(buf);

    // 4) 상속 체인 출력
    UE_LOG("[RTTI] Inheritance chain: ");
    for (const UClass* c = Obj->GetClass(); c; c = c->Super)
    {
        std::snprintf(buf, sizeof(buf), "%s%s", c->Name, c->Super ? " <- " : "\r\n");
        UE_LOG(buf);
    }
    //FString Name = Obj->GetName();
    std::snprintf(buf, sizeof(buf), "[RTTI] TypeName = %s\r\n", Obj->GetName().c_str());
    OutputDebugStringA(buf);
    OutputDebugStringA("================================\r\n");
}

void UWorld::Initialize()
{
    FObjManager::Preload();

    // 새 씬 생성
    CreateNewScene();

    InitializeMainCamera();
    InitializeGrid();
    InitializeGizmo();

    // Fullscreen quad 초기화 추가
    InitializeFullscreenQuad();

    // BVH 초기화 (빈 상태로 시작)
    if (!BVH)
    {
        BVH = new FBVH();
    }

    // 액터 간 참조 설정
    //SetupActorReferences();
}

void UWorld::InitializeMainCamera() 
{
    MainCameraActor = NewObject<ACameraActor>();

    DebugRTTI_UObject(MainCameraActor, "MainCameraActor");
    UIManager.SetCamera(MainCameraActor);

    EngineActors.Add(MainCameraActor);
}

void UWorld::InitializeGrid()
{
    GridActor = NewObject<AGridActor>();
    GridActor->Initialize();

    // Add GridActor to Actors array so it gets rendered in the main loop
    EngineActors.push_back(GridActor);
    //EngineActors.push_back(GridActor);
}

void UWorld::InitializeGizmo()
{
    // === 기즈모 엑터 초기화 ===
    GizmoActor = NewObject<AGizmoActor>();
    GizmoActor->SetWorld(this);
    GizmoActor->SetActorTransform(FTransform(FVector{0, 0, 0},
                                             FQuat::MakeFromEuler(FVector{0, -90, 0}),
                                             FVector{1, 1, 1}));
    // 기즈모에 카메라 참조 설정
    if (MainCameraActor)
    {
        GizmoActor->SetCameraActor(MainCameraActor);
    }

    UIManager.SetGizmoActor(GizmoActor);
}

void UWorld::InitializeSceneGraph(TArray<AActor*>& Actors)
{
    Octree = NewObject<UOctree>();
    //	Octree->Initialize(FBound({ -100,-100,-100 }, { 100,100,100 }));
    //const TArray<AActor*>& InActors, FBound& WorldBounds, int32 Depth = 0
    Octree->Build(Actors, FBound({-100, -100, -100}, {100, 100, 100}), 0);

    // 빌드 완료 후 모든 마이크로 BVH 미리 생성
    Octree->PreBuildAllMicroBVH();

    // BVH 초기화 및 빌드
    BVH = new FBVH();
    BVH->Build(Actors);
}

void UWorld::RenderSceneGraph()
{
    if (!Octree)
    {
        return;
    }
    Octree->Render(nullptr);
}

void UWorld::SetRenderer(URenderer* InRenderer)
{
    Renderer = InRenderer;
}

void UWorld::Render()
{
    Renderer->BeginFrame();
    UIManager.Render();

    // UIManager의 뷰포트 전환 상태에 따라 렌더링 변경 SWidget으로 변경해줄거임

    if (MultiViewport)
    {
        MultiViewport->OnRender();
    }
     
    PostProcessing(); 

    //프레임 종료 
    Renderer->EndFrame();
    UIManager.EndFrame();
    Renderer->GetRHIDevice()->Present();
}

void UWorld::RenderViewports(ACameraActor* Camera, FViewport* Viewport)
{
    // 뷰포트의 실제 크기로 aspect ratio 계산
    float ViewportAspectRatio = static_cast<float>(Viewport->GetSizeX()) / static_cast<float>(
        Viewport->GetSizeY());
    if (Viewport->GetSizeY() == 0)
    {
        ViewportAspectRatio = 1.0f;
    } // 0으로 나누기 방지

    FMatrix ViewMatrix = Camera->GetViewMatrix();
    FMatrix ProjectionMatrix = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);
    if (!Renderer)
    {
        return;
    }
   FVector rgb(1.0f, 1.0f, 1.0f);

    FFrustum ViewFrustum;
    ViewFrustum.Update(ViewMatrix * ProjectionMatrix);

    Renderer->BeginLineBatch();

    // ====================================================================
    // View Mode 설정
    // ====================================================================
    Renderer->SetViewModeType(ViewModeIndex);

    int AllActorCount = 0;
    int FrustumCullCount = 0;
    int32 TotalDecalCount = 0;

    // ====================================================================
    // Pass 0: Visible Lights 를 Pruning하는 과정
    // ====================================================================
    {
        TArray<FLightInfo> VisibleFrameLights;

        FHeatInfo HeatCB; // 새로 추가: 이 뷰포트용 HeatCB
        D3D11RHI* RHI = static_cast<D3D11RHI*>(Renderer->GetRHIDevice());
        IDXGISwapChain* SwapChain = RHI->GetSwapChain();
        DXGI_SWAP_CHAIN_DESC swapDesc;
        SwapChain->GetDesc(&swapDesc);
        HeatCB.InvRes = FVector2D(1.0f / swapDesc.BufferDesc.Width, 1.0f / swapDesc.BufferDesc.Height);
        HeatCB.FalloffExp = 2.2f;   // 시작값
        HeatCB.DistortionPx = 0.8f;   // 시작값
        HeatCB.EmissiveMul = 0.12f;  // 시작값
        HeatCB.TimeSec =  GetTimeSeconds(); /*엔진 시간 전달*/ // UTime::NowSeconds(); //TODO

        const TArray<AActor*>& Actors = Level ? Level->GetActors() : TArray<AActor*>();

        for (AActor* Actor : Actors)
        {
            if (!Actor || Actor->GetActorHiddenInGame()) continue;

            for (UActorComponent* Comp : Actor->GetComponents())
            {
                USceneComponent* SceneComp = Cast<USceneComponent>(Comp);
                if (SceneComp == nullptr) continue;

                if (UPointLightComponent* PointLightComp = Cast<UPointLightComponent>(SceneComp))
                {
                    FLightInfo LightInfo;
                    LightInfo.Type = ELighType::Point;

                    LightInfo.LightPos = PointLightComp->GetWorldLocation();
                    LightInfo.Radius = PointLightComp->GetAttenuationRadius();
                    LightInfo.RadiusFallOff = PointLightComp->GetFalloff();
                    LightInfo.Color = PointLightComp->GetLightColor();
                    LightInfo.Intensity = PointLightComp->GetIntensity();
                    
                    if (VisibleFrameLights.size() < 8)
                    {
                        VisibleFrameLights.Add(LightInfo);
                    }


                    if (HeatCB.NumSpots < 8)
                    {
                        FVector2D OutUV;
                        float OutViewZ = 0.0f;
                        if (!WorldToScreenOutViewZ(LightInfo.LightPos, ViewMatrix, ProjectionMatrix, OutUV, OutViewZ))
                            continue; // clip.W<=0 혹은 기타 예외

                        // 화면 밖이면 스킵(선택): uv clamp해서 낮은 strength로 처리하는 옵션도 가능
                        if (OutUV.X < 0.f || OutUV.X > 1.f || OutUV.Y < 0.f || OutUV.Y > 1.f)
                            continue;

                        float RadiusPx = 0.0f;

                        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();
                        RadiusPx = WorldRadiusToPixelRadius_Persp(LightInfo.Radius, OutViewZ, Cam->GetFOV() * PI/ 180 , MainViewport->GetHeight());

                        //float RadiusPx = 120.0f;
                        float Strength = FMath::Clamp(LightInfo.Intensity * 0.5f, 0.0f, 1.0f);
                        
                        FHeatSpot Heat{};
                        Heat.UV = OutUV;
                        Heat.RadiusPx = RadiusPx;
                        Heat.Strength = Strength;
                        HeatCB.Spots[HeatCB.NumSpots++] = Heat; 
                    }

                }
            
            }
        }

        Renderer->SetWorldLights(VisibleFrameLights);
        Renderer->UpdateLightBuffer();
        Renderer->UpdateHeatConstantBuffer(HeatCB); //TODO 
    }


    const TArray<AActor*>& LevelActors = Level ? Level->GetActors() : TArray<AActor*>();

    // ====================================================================
    // Pass 1: 일반 렌더링 - Depth Buffer 채우기
    // ====================================================================
    
    for (AActor* Actor : LevelActors)
    {
        if (!Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_Primitives))
        {
            continue;
        }
        if (!Actor)
        {
            continue;
        }
        if (Actor->GetActorHiddenInGame())
        {
            continue;
        }

        AllActorCount++;
        for (UActorComponent* Component : Actor->GetComponents())
        {
            if (!Component)
            {
                continue;
            }

            if (UActorComponent* ActorComp = Cast<UActorComponent>(Component))
            {
                if (!ActorComp->IsActive())
                {
                    continue;
                }
            }

            if (Cast<UTextRenderComponent>(Component) &&
                !Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_BillboardText))
            {
                continue;
            }

            // Decal Component는 Editor Visuals만 렌더링
            if (UDecalComponent* DecalComp = Cast<UDecalComponent>(Component))
            {
                DecalComp->RenderEditorVisuals(Renderer, ViewMatrix, ProjectionMatrix);
                TotalDecalCount++;
                continue;
            }

            if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
            {
                bool bIsSelected = SelectionManager.IsActorSelected(Actor);
                Renderer->UpdateHighLightConstantBuffer(bIsSelected, rgb, 0, 0, 0, 0);
                Primitive->Render(Renderer, ViewMatrix, ProjectionMatrix, Viewport);
            }
        }
        Renderer->OMSetBlendState(false);
    }

    // 엔진 액터들 (그리드 등) 렌더링
    RenderEngineActors(ViewMatrix, ProjectionMatrix, Viewport);

    URenderingStatsCollector& StatsCollector = URenderingStatsCollector::GetInstance();

    // ====================================================================
    // Pass 2: Decal Projection Pass (Scene Depth 모드가 아닐 때만)
    // ====================================================================
    if (ViewModeIndex != EViewModeIndex::VMI_SceneDepth)
    {
        StatsCollector.BeginDecalPass();
        FDecalRenderingStats& DecalStats = StatsCollector.GetDecalStats();
        DecalStats.TotalDecalCount = TotalDecalCount;

        if (Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_Primitives) &&
            Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_Decals) &&
            Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes))
        {
            for (AActor* Actor : LevelActors)
            {
                if (!Actor || Actor->GetActorHiddenInGame())
                {
                    continue;
                }

                for (UActorComponent* Component : Actor->GetComponents())
                {
                    if (UDecalComponent* DecalComp = Cast<UDecalComponent>(Component))
                    {
                        DecalComp->RenderDecalProjection(Renderer, ViewMatrix, ProjectionMatrix);
                    }
                }
            }
        }
        StatsCollector.EndDecalPass();
    }

    Renderer->EndLineBatch(FMatrix::Identity(), ViewMatrix, ProjectionMatrix);

    // ====================================================================
    // Pass 3: Post-Process - Scene Depth 시각화 (VMI_SceneDepth 모드일 때만)
    // ====================================================================
    if (ViewModeIndex == EViewModeIndex::VMI_SceneDepth)
    {
        RenderSceneDepthPass(ViewMatrix, ProjectionMatrix, Viewport);
    }
}

void UWorld::RenderEngineActors(const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix, FViewport* Viewport)
{
    for (AActor* EngineActor : EngineActors)
    {
        if (!EngineActor)
        {
            continue;
        }

        if (EngineActor->GetActorHiddenInGame())
        {
            continue;
        }

        if (Cast<AGridActor>(EngineActor) && !Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_Grid))
        {
            continue;
        }

        for (UActorComponent* Component : EngineActor->GetComponents())
        {
            if (!Component)
            {
                continue;
            }

            if (UActorComponent* ActorComp = Cast<UActorComponent>(Component))
            {
                if (!ActorComp->IsActive())
                {
                    continue;
                }
            }

            if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
            {
                Renderer->SetViewModeType(ViewModeIndex);
                Primitive->Render(Renderer, ViewMatrix, ProjectionMatrix, Viewport);
                Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual);
            }
        }
        Renderer->OMSetBlendState(false);
    }
}

void UWorld::ApplyFXAA(FViewport* vt)
{
    // rtv는 backbuffer로 설정되어있음 
    UShader* FXAAShader = UResourceManager::GetInstance().Load<UShader>("FXAA.hlsl");

    // Update viewport CB (b6) and bind backbuffer for FXAA 
    Renderer->UpdateViewportBuffer(vt->GetStartX(), vt->GetStartY(), vt->GetSizeX(), vt->GetSizeY());


    Renderer->PrepareShader(FXAAShader);
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);

    ID3D11DeviceContext* DevieContext = Renderer->GetRHIDevice()->GetDeviceContext();
    // Set FXAA shader (uses SV_VertexID, no input layout)
    DevieContext->VSSetShader(FXAAShader->GetVertexShader(), nullptr, 0);
    DevieContext->PSSetShader(FXAAShader->GetPixelShader(), nullptr, 0);
    DevieContext->IASetInputLayout(FXAAShader->GetInputLayout());
    DevieContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // Ensure no blending for fullscreen resolve
    Renderer->OMSetBlendState(false);

    // Bind source color as t0
    ID3D11ShaderResourceView* HeatSRV = static_cast<D3D11RHI*>(Renderer->GetRHIDevice())->GetHeatSRV();
  
    Renderer->GetRHIDevice()->PSSetDefaultSampler(0);
    DevieContext->PSSetShaderResources(0, 1, &HeatSRV);

    // Draw fullscreen triangle
    DevieContext->Draw(3, 0);

    // Unbind SRV to avoid warnings on next frame when rebinding as RTV
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };

    DevieContext->PSSetShaderResources(0, 1, nullSRV); 
}

void UWorld::ApplyHeat(FViewport* vt)
{
    Renderer->EnsurePostProcessingShader(); 
    UShader* HeatShader = Renderer->GetHeatShader();

    // Update viewport CB (b6)
    Renderer->UpdateViewportBuffer(vt->GetStartX(), vt->GetStartY(), vt->GetSizeX(), vt->GetSizeY());

    Renderer->PrepareShader(HeatShader);
    Renderer->OMSetDepthStencilState(EComparisonFunc::Always); // Post-processing doesn't need depth test.

    ID3D11DeviceContext* DeviceContext = Renderer->GetRHIDevice()->GetDeviceContext();

    ID3D11RenderTargetView* HeatRTV = static_cast<D3D11RHI*>(Renderer->GetRHIDevice())->GetHeatRTV();
    DeviceContext->OMSetRenderTargets(1, &HeatRTV, nullptr);
    
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Renderer->OMSetBlendState(false);

    // Input scene texture from previous pass
    ID3D11ShaderResourceView* SceneSRV = static_cast<D3D11RHI*>(Renderer->GetRHIDevice())->GetFXAASRV();
    DeviceContext->PSSetShaderResources(0, 1, &SceneSRV);

    // Noise texture for distortion
    FTextureData* NoiseTexData = UResourceManager::GetInstance().CreateOrGetTextureData("./Data/WorleyNoise.jpg");
    //FTextureData* NoiseTexData = UResourceManager::GetInstance().CreateOrGetTextureData("./Data/PerlinNoise.png");
    DeviceContext->PSSetShaderResources(1, 1, &NoiseTexData->TextureSRV);

    Renderer->GetRHIDevice()->PSSetMirrorSampler(0);

    DeviceContext->Draw(3, 0);

    // Unbind SRVs
    ID3D11ShaderResourceView* NullSRV[2] = { nullptr, nullptr };
    DeviceContext->PSSetShaderResources(0, 2, NullSRV);
}

void UWorld::Tick(float DeltaSeconds)
{
    TimeSeconds += DeltaSeconds;

    // Level의 Actors Tick
    if (Level)
    {
        for (AActor* Actor : Level->GetActors())
        {
            if (Actor && Actor->IsActorTickEnabled())
            {
               Actor->Tick(DeltaSeconds);
            }

            // Actor의 Tick이 끝난 후에 
            // Decal과 충돌한 Actor를 수집한다. 
            //for (ADecalActor* DecalActor: Level->GetDecalActors())
            //{
            //     
            //    DecalActor->CheckAndAddOverlappingActors(Actor);
            //}
             
        }
    }

    // Engine Actors Tick
    for (AActor* EngineActor : EngineActors)
    {
        if (EngineActor && EngineActor->IsActorTickEnabled())
        {
            EngineActor->Tick(DeltaSeconds);
        }
    }

    if (GizmoActor)
    {
        GizmoActor->Tick(DeltaSeconds);
    }

    //ProcessActorSelection();
    ProcessViewportInput();
    //Input Manager가 카메라 후에 업데이트 되어야함

    // 뷰포트 업데이트 - UIManager의 뷰포트 전환 상태에 따라
    if (MultiViewport)
    {
        MultiViewport->OnUpdate(DeltaSeconds);
    }

    //InputManager.Update();
    UIManager.Update(DeltaSeconds);

    // BVH 업데이트 (Transform 변경이 있을 경우)
    UpdateBVHIfNeeded();
}

float UWorld::GetTimeSeconds() const
{
    return TimeSeconds;
}

bool UWorld::FrustumCullActors(const FFrustum& ViewFrustum, const AActor* Actor, int & FrustumCullCount)
{
    if (Actor->CollisionComponent)
    {
        FBound Test = Actor->CollisionComponent->GetWorldBoundFromCube();

        // 절두체 밖에 있다면, 이 액터의 렌더링 과정을 모두 건너뜁니다.
        if (!ViewFrustum.IsVisible(Test))
        {
            FrustumCullCount++;
            return true;
        }
    }

	return false;
}

FString UWorld::GenerateUniqueActorName(const FString& ActorType)
{
    // Get current count for this type
    int32& CurrentCount = ObjectTypeCounts[ActorType];
    FString UniqueName = ActorType + "_" + std::to_string(CurrentCount);
    CurrentCount++;
    return UniqueName;
}

//
// 액터 제거
//
bool UWorld::DestroyActor(AActor* Actor)
{
    if (!Actor)
    {
        return false; // nullptr 들어옴 → 실패
    }

    // SelectionManager에서 선택 해제 (메모리 해제 전에 하자)
    USelectionManager::GetInstance().DeselectActor(Actor);

    // UIManager에서 픽된 액터 정리
    if (UIManager.GetPickedActor() == Actor)
    {
        UIManager.ResetPickedActor();
    }

    // 배열에서  제거 시도
    // Level에서 제거 시도
    if (Level)
    {
        Level->RemoveActor(Actor);

        // 메모리 해제
        ObjectFactory::DeleteObject(Actor);
        // 삭제된 액터 정리
        USelectionManager::GetInstance().CleanupInvalidActors();

        // BVH 더티 플래그 설정
        MarkBVHDirty();

        return true; // 성공적으로 삭제
    }

    return false; // 월드에 없는 액터
}

inline FString ToObjFileName(const FString& TypeName)
{
    return "Data/" + TypeName + ".obj";
}

inline FString RemoveObjExtension(const FString& FileName)
{
    const FString Extension = ".obj";

    // 마지막 경로 구분자 위치 탐색 (POSIX/Windows 모두 지원)
    const uint64 Sep = FileName.find_last_of("/\\");
    const uint64 Start = (Sep == FString::npos) ? 0 : Sep + 1;

    // 확장자 제거 위치 결정
    uint64 End = FileName.size();
    if (End >= Extension.size() &&
        FileName.compare(End - Extension.size(), Extension.size(), Extension) == 0)
    {
        End -= Extension.size();
    }

    // 베이스 이름(확장자 없는 파일명) 반환
    if (Start <= End)
    {
        return FileName.substr(Start, End - Start);
    }

    // 비정상 입력 시 원본 반환 (안전장치)
    return FileName;
}

void UWorld::CreateNewScene()
{
    // Safety: clear interactions that may hold stale pointers
    SelectionManager.ClearSelection();
    UIManager.ResetPickedActor();
    // Level의 Actors 정리
    if (Level)
    {
        for (AActor* Actor : Level->GetActors())
        {
            ObjectFactory::DeleteObject(Actor);
        }
        Level->GetActors().clear();
    }

    if (Octree)
    {
        Octree->Release();//새로운 씬이 생기면 Octree를 지워준다.
    }
    if (BVH)
    {
        BVH->Clear();//새로운 씬이 생기면 BVH를 지워준다.
    }
    // 이름 카운터 초기화: 씬을 새로 시작할 때 각 BaseName 별 suffix를 0부터 다시 시작
    ObjectTypeCounts.clear();
}

// 액터 인터페이스 관리 메소드들
void UWorld::SetupActorReferences()
{
    /*if (GizmoActor && MainCameraActor)
    {
        GizmoActor->SetCameraActor(MainCameraActor);
    }*/
}

//마우스 피킹관련 메소드
void UWorld::ProcessActorSelection()
{
    if (InputManager.IsMouseButtonPressed(LeftButton))
    {
        const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
        {
            if (MultiViewport)
            {
                MultiViewport->OnMouseDown(MousePosition, 0);
            }
        }
    }
    if (InputManager.IsMouseButtonPressed(RightButton))
    {
        const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
        {
            if (MultiViewport)
            {
                MultiViewport->OnMouseDown(MousePosition, 0);
            }
        }
    }
    if (InputManager.IsMouseButtonPressed(RightButton))
    {
        const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
        {
            if (MultiViewport)
            {
                MultiViewport->OnMouseDown(MousePosition, 1);
            }
        }
    }
    if (InputManager.IsMouseButtonReleased(RightButton))
    {
        const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
        {
            if (MultiViewport)
            {
                MultiViewport->OnMouseUp(MousePosition, 1);
            }
        }
    }
}

void UWorld::ProcessViewportInput()
{
    const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();

    if (InputManager.IsMouseButtonPressed(LeftButton))
    {
        const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
        {
            if (MultiViewport)
            {
                MultiViewport->OnMouseDown(MousePosition, 0);
            }
        }
    }
    if (InputManager.IsMouseButtonPressed(RightButton))
    {
        const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
        {
            if (MultiViewport)
            {
                MultiViewport->OnMouseDown(MousePosition, 1);
            }
        }
    }
    if (InputManager.IsMouseButtonReleased(LeftButton))
    {
        const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
        {
            if (MultiViewport)
            {
                MultiViewport->OnMouseUp(MousePosition, 0);
            }
        }
    }
    if (InputManager.IsMouseButtonReleased(RightButton))
    {
        const FVector2D MousePosition = UInputManager::GetInstance().GetMousePosition();
        {
            if (MultiViewport)
            {
                MultiViewport->OnMouseUp(MousePosition, 1);
            }
        }
    }
    MultiViewport->OnMouseMove(MousePosition);
}

void UWorld::LoadScene(const FString& SceneName)
{
    namespace fs = std::filesystem;
    fs::path path = fs::path("Scene") / SceneName;
    if (path.extension().string() != ".Scene")
    {
        path.replace_extension(".Scene");
    }

    const FString FilePath = path.make_preferred().string();

    // [1] 로드 시작 전 현재 카운터 백업
    const uint32 PreLoadNext = UObject::PeekNextUUID();

    // [2] 파일 NextUUID는 현재보다 클 때만 반영(절대 하향 설정 금지)
    uint32 LoadedNextUUID = 0;
    if (FSceneLoader::TryReadNextUUID(FilePath, LoadedNextUUID))
    {
        if (LoadedNextUUID > UObject::PeekNextUUID())
        {
            UObject::SetNextUUID(LoadedNextUUID);
        }
    }

    // [3] 기존 씬 비우기
    CreateNewScene();

    // [4] 로드
    FPerspectiveCameraData CamData{};
    const TArray<FPrimitiveData>& Primitives = FSceneLoader::Load(FilePath, &CamData);

    // 마우스 델타 초기화
    const FVector2D CurrentMousePos = UInputManager::GetInstance().GetMousePosition();
    UInputManager::GetInstance().SetLastMousePosition(CurrentMousePos);

    // 카메라 적용
    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();

        // 위치/회전(월드 트랜스폼)
        MainCameraActor->SetActorLocation(CamData.Location);
        MainCameraActor->SetActorRotation(FQuat::MakeFromEuler(CamData.Rotation));

        // 입력 경로와 동일한 방식으로 각도/회전 적용
        // 매핑: Pitch = CamData.Rotation.Y, Yaw = CamData.Rotation.Z
        MainCameraActor->SetAnglesImmediate(CamData.Rotation.Y, CamData.Rotation.Z);

        // UIManager의 카메라 회전 상태도 동기화
        UIManager.UpdateMouseRotation(CamData.Rotation.Y, CamData.Rotation.Z);

        // 프로젝션 파라미터
        Cam->SetFOV(CamData.FOV);
        Cam->SetClipPlanes(CamData.NearClip, CamData.FarClip);

        // UI 위젯에 현재 카메라 상태로 재동기화 요청
        UIManager.SyncCameraControlFromCamera();
    }

    // 1) 현재 월드에서 이미 사용 중인 UUID 수집(엔진 액터 + 기즈모)
    std::unordered_set<uint32> UsedUUIDs;
    auto AddUUID = [&](AActor* A) { if (A) UsedUUIDs.insert(A->UUID); };
    for (AActor* Eng : EngineActors)
    {
        AddUUID(Eng);
    }
    AddUUID(GizmoActor); // Gizmo는 EngineActors에 안 들어갈 수 있으므로 명시 추가

    uint32 MaxAssignedUUID = 0;

    for (const FPrimitiveData& Primitive : Primitives)
    {
        // 스폰 시 필요한 초기 트랜스폼은 그대로 넘김
        AStaticMeshActor* StaticMeshActor = SpawnActor<AStaticMeshActor>(
            FTransform(Primitive.Location,
                       SceneRotUtil::QuatFromEulerZYX_Deg(Primitive.Rotation),
                       Primitive.Scale));

        // 스폰 시점에 자동 발급된 고유 UUID (충돌 시 폴백으로 사용)
        uint32 Assigned = StaticMeshActor->UUID;

        // 우선 스폰된 UUID를 사용 중으로 등록
        UsedUUIDs.insert(Assigned);

        // 2) 파일의 UUID를 우선 적용하되, 충돌이면 스폰된 UUID 유지
        if (Primitive.UUID != 0)
        {
            if (UsedUUIDs.find(Primitive.UUID) == UsedUUIDs.end())
            {
                // 스폰된 ID 등록 해제 후 교체
                UsedUUIDs.erase(Assigned);
                StaticMeshActor->UUID = Primitive.UUID;
                Assigned = Primitive.UUID;
                UsedUUIDs.insert(Assigned);
            }
            else
            {
                // 충돌: 파일 UUID 사용 불가 → 경고 로그 및 스폰된 고유 UUID 유지
                UE_LOG("LoadScene: UUID collision detected (%u). Keeping generated %u for actor.",
                       Primitive.UUID, Assigned);
            }
        }

        MaxAssignedUUID = std::max(MaxAssignedUUID, Assigned);

        if (UStaticMeshComponent* SMC = StaticMeshActor->GetStaticMeshComponent())
        {
            FPrimitiveData Temp = Primitive;
            SMC->Serialize(true, Temp);

            FString LoadedAssetPath;
            if (UStaticMesh* Mesh = SMC->GetStaticMesh())
            {
                LoadedAssetPath = Mesh->GetAssetPathFileName();
            }

            if (LoadedAssetPath == "Data/Sphere.obj")
            {
                StaticMeshActor->SetCollisionComponent(EPrimitiveType::Sphere);
            }
            else
            {
                StaticMeshActor->SetCollisionComponent();
            }

            FString BaseName = "StaticMesh";
            if (!LoadedAssetPath.empty())
            {
                BaseName = RemoveObjExtension(LoadedAssetPath);
            }
            StaticMeshActor->SetName(GenerateUniqueActorName(BaseName));
        }
    }

 

    // 3) 최종 보정: 전역 카운터는 절대 하향 금지 + 현재 사용된 최대값 이후로 설정
    const uint32 DuringLoadNext = UObject::PeekNextUUID();
    const uint32 SafeNext = std::max({DuringLoadNext, MaxAssignedUUID + 1, PreLoadNext});
    UObject::SetNextUUID(SafeNext);

    if (Level)
    {
        InitializeSceneGraph(Level->GetActors());
    }
}

void UWorld::SaveScene(const FString& SceneName)
{
    TArray<FPrimitiveData> Primitives;

    for (AActor* Actor :Level->GetActors())
    {
        if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor))
        {
            FPrimitiveData Data;
            Data.UUID = Actor->UUID;
            Data.Type = "StaticMeshComp";
            if (UStaticMeshComponent* SMC = MeshActor->GetStaticMeshComponent())
            {
                SMC->Serialize(false, Data); // 여기서 RotUtil 적용됨(상위 Serialize)
            }
            Primitives.push_back(Data);
        }
        else
        {
            FPrimitiveData Data;
            Data.UUID = Actor->UUID;
            Data.Type = "Actor";

            if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
            {
                Prim->Serialize(false, Data); // 여기서 RotUtil 적용됨
            }
            else
            {
                // 루트가 Primitive가 아닐 때도 동일 규칙으로 저장
                Data.Location = Actor->GetActorLocation();
                Data.Rotation = SceneRotUtil::EulerZYX_Deg_FromQuat(Actor->GetActorRotation());
                Data.Scale = Actor->GetActorScale();
            }

            Data.ObjStaticMeshAsset.clear();
            Primitives.push_back(Data);
        }
    }

    // 카메라 데이터 채우기
    const FPerspectiveCameraData* CamPtr = nullptr;
    FPerspectiveCameraData CamData;
    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();

        CamData.Location = MainCameraActor->GetActorLocation();

        // 내부 누적 각도로 저장: Pitch=Y, Yaw=Z, Roll=0
        CamData.Rotation.X = 0.0f;
        CamData.Rotation.Y = MainCameraActor->GetCameraPitch();
        CamData.Rotation.Z = MainCameraActor->GetCameraYaw();

        CamData.FOV = Cam->GetFOV();
        CamData.NearClip = Cam->GetNearClip();
        CamData.FarClip = Cam->GetFarClip();
        CamPtr = &CamData;
    }

    // Scene 디렉터리에 저장
    FSceneLoader::Save(Primitives, CamPtr, SceneName);
}

void UWorld::SaveSceneV2(const FString& SceneName)
{
    FSceneData SceneData;
    SceneData.Version = 2;
    SceneData.NextUUID = UObject::PeekNextUUID();

    // 카메라 데이터 채우기
    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();
        SceneData.Camera.Location = MainCameraActor->GetActorLocation();
        SceneData.Camera.Rotation.X = 0.0f;
        SceneData.Camera.Rotation.Y = MainCameraActor->GetCameraPitch();
        SceneData.Camera.Rotation.Z = MainCameraActor->GetCameraYaw();
        SceneData.Camera.FOV = Cam->GetFOV();
        SceneData.Camera.NearClip = Cam->GetNearClip();
        SceneData.Camera.FarClip = Cam->GetFarClip();
    }

    // Actor 및 Component 계층 수집
    for (AActor* Actor : Level->GetActors())
    {
        if (!Actor) continue;

        // Actor 데이터
        FActorData ActorData;
        ActorData.UUID = Actor->UUID;
        ActorData.Name = Actor->GetName().ToString();
        ActorData.Type = Actor->GetClass()->Name;

        if (Actor->GetRootComponent())
            ActorData.RootComponentUUID = Actor->GetRootComponent()->UUID;

        SceneData.Actors.push_back(ActorData);

        // OwnedComponents 순회 (모든 컴포넌트 포함)
        for (UActorComponent* ActorComp : Actor->GetComponents())
        {
            if (!ActorComp) continue;

            FComponentData CompData;
            CompData.UUID = ActorComp->UUID;
            CompData.OwnerActorUUID = Actor->UUID;
            CompData.Type = ActorComp->GetClass()->Name;

            // SceneComponent인 경우 Transform과 계층 구조 정보 저장
            if (USceneComponent* Comp = Cast<USceneComponent>(ActorComp))
            {
                // 부모 컴포넌트 UUID (RootComponent면 0)
                if (Comp->GetAttachParent())
                    CompData.ParentComponentUUID = Comp->GetAttachParent()->UUID;
                else
                    CompData.ParentComponentUUID = 0;

                // Transform
                CompData.RelativeLocation = Comp->GetRelativeLocation();
                CompData.RelativeRotation = Comp->GetRelativeRotation().ToEuler();
                CompData.RelativeScale = Comp->GetRelativeScale();

                // Type별 속성
                if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Comp))
                {
                    if (StaticMeshComponent->GetStaticMesh())
                    {
                        CompData.StaticMesh = StaticMeshComponent->GetStaticMesh()->GetAssetPathFileName();
                        UE_LOG("SaveScene: StaticMesh saved: %s", CompData.StaticMesh.c_str());
                    }
                    else
                    {
                        UE_LOG("SaveScene: StaticMeshComponent has no StaticMesh assigned");
                    }
                    // TODO: Materials 수집
                }
                else if (UDecalComponent* DecalComp = Cast<UDecalComponent>(Comp))
                {
                    // DecalComponent 속성 저장
                    if (DecalComp->GetDecalTexture())
                    {
                        CompData.DecalTexture = DecalComp->GetDecalTexture()->GetFilePath();
                    }
                    CompData.DecalSize = DecalComp->GetDecalSize();
                    CompData.FadeInDuration = DecalComp->GetFadeInDuration();
                    CompData.FadeStartDelay = DecalComp->GetFadeStartDelay();
                    CompData.FadeDuration = DecalComp->GetFadeDuration();
                    CompData.bIsOrthoMatrix = DecalComp->GetOrthoMatrixFlag();
                }
                else if (UBillboardComponent* BillboardComp = Cast<UBillboardComponent>(Comp))
                {
                    // BillboardComponent 속성 저장
                    CompData.BillboardTexturePath = BillboardComp->GetTexturePath();
                    CompData.BillboardWidth = BillboardComp->GetBillboardWidth();
                    CompData.BillboardHeight = BillboardComp->GetBillboardHeight();
                    CompData.UCoord = BillboardComp->GetU();
                    CompData.VCoord = BillboardComp->GetV();
                    CompData.ULength = BillboardComp->GetUL();
                    CompData.VLength = BillboardComp->GetVL();
                    CompData.bIsScreenSizeScaled = BillboardComp->IsScreenSizeScaled();
                    CompData.ScreenSize = BillboardComp->GetScreenSize();
                }
            }
            else
            {
                // ActorComponent (Transform 없음)
                CompData.ParentComponentUUID = 0;

                // MovementComponent 속성 저장
                if (UMovementComponent* MovementComp = Cast<UMovementComponent>(ActorComp))
                {
                    CompData.Velocity = MovementComp->GetVelocity();
                    CompData.Acceleration = MovementComp->GetAcceleration();
                    CompData.bUpdateOnlyIfRendered = MovementComp->GetUpdateOnlyIfRendered();

                    // RotatingMovementComponent 추가 속성 저장
                    if (URotatingMovementComponent* RotatingComp = Cast<URotatingMovementComponent>(MovementComp))
                    {
                        CompData.RotationRate = RotatingComp->GetRotationRate();
                        CompData.PivotTranslation = RotatingComp->GetPivotTranslation();
                        CompData.bRotationInLocalSpace = RotatingComp->IsRotationInLocalSpace();
                    }
                    // ProjectileMovementComponent 추가 속성 저장
                    else if (UProjectileMovementComponent* ProjectileComp = Cast<UProjectileMovementComponent>(MovementComp))
                    {
                        CompData.Gravity = ProjectileComp->GetGravity();
                        CompData.InitialSpeed = ProjectileComp->GetInitialSpeed();
                        CompData.MaxSpeed = ProjectileComp->GetMaxSpeed();
                        CompData.HomingAccelerationMagnitude = ProjectileComp->GetHomingAccelerationMagnitude();
                        CompData.bIsHomingProjectile = ProjectileComp->IsHomingProjectile();
                        CompData.bRotationFollowsVelocity = ProjectileComp->GetRotationFollowsVelocity();
                        CompData.ProjectileLifespan = ProjectileComp->GetProjectileLifespan();
                        CompData.bAutoDestroyWhenLifespanExceeded = ProjectileComp->GetAutoDestroyWhenLifespanExceeded();
                        CompData.bIsActive = ProjectileComp->IsActive();
                    }
                }
            }

            SceneData.Components.push_back(CompData);
        }
    }

    // Scene 디렉터리에 V2 포맷으로 저장
    FSceneLoader::SaveV2(SceneData, SceneName);
}

void UWorld::LoadSceneV2(const FString& SceneName)
{
    namespace fs = std::filesystem;
    fs::path path = fs::path("Scene") / SceneName;
    if (path.extension().string() != ".Scene")
    {
        path.replace_extension(".Scene");
    }

    const FString FilePath = path.make_preferred().string();

    // NextUUID 업데이트
    uint32 LoadedNextUUID = 0;
    if (FSceneLoader::TryReadNextUUID(FilePath, LoadedNextUUID))
    {
        if (LoadedNextUUID > UObject::PeekNextUUID())
        {
            UObject::SetNextUUID(LoadedNextUUID);
        }
    }

    // 기존 씬 비우기
    CreateNewScene();

    // V2 데이터 로드
    FSceneData SceneData = FSceneLoader::LoadV2(FilePath);

    // 마우스 델타 초기화
    const FVector2D CurrentMousePos = UInputManager::GetInstance().GetMousePosition();
    UInputManager::GetInstance().SetLastMousePosition(CurrentMousePos);

    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();
        MainCameraActor->SetActorLocation(SceneData.Camera.Location);
        MainCameraActor->SetCameraPitch(SceneData.Camera.Rotation.Y);
        MainCameraActor->SetCameraYaw(SceneData.Camera.Rotation.Z);

        // 입력 경로와 동일한 방식으로 각도/회전 적용
      // 매핑: Pitch = CamData.Rotation.Y, Yaw = CamData.Rotation.Z
        MainCameraActor->SetAnglesImmediate(SceneData.Camera.Rotation.Y, SceneData.Camera.Rotation.Z);

        // UIManager의 카메라 회전 상태도 동기화
        UIManager.UpdateMouseRotation(SceneData.Camera.Rotation.Y, SceneData.Camera.Rotation.Z);

        Cam->SetFOV(SceneData.Camera.FOV);
        Cam->SetClipPlanes(SceneData.Camera.NearClip, SceneData.Camera.FarClip);

        // UI 위젯에 현재 카메라 상태로 재동기화 요청
        UIManager.SyncCameraControlFromCamera();
      
    }

    // UUID → Object 매핑 테이블
    TMap<uint32, AActor*> ActorMap;
    TMap<uint32, USceneComponent*> ComponentMap;

    // ========================================
    // Pass 1: Actor 및 Component 생성
    // ========================================
    for (const FActorData& ActorData : SceneData.Actors)
    {
        AActor* NewActor = Cast<AActor>(NewObject(ActorData.Type));

        if (!NewActor)
        {
            UE_LOG("Failed to create Actor: %s", ActorData.Type.c_str());
            continue;
        }

        NewActor->UUID = ActorData.UUID;
        NewActor->SetName(ActorData.Name);
        NewActor->SetWorld(this);

        // DecalActor의 경우 생성자가 만든 DecalComponent를 삭제
        if (ADecalActor* DecalActor = Cast<ADecalActor>(NewActor))
        {
            DecalActor->ClearDefaultComponents();
        }
        // StaticMeshActor의 경우 생성자가 만든 컴포넌트들을 삭제
        else if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(NewActor))
        {
            StaticMeshActor->ClearDefaultComponents();
        }

        ActorMap.Add(ActorData.UUID, NewActor);
    }

    // Component 생성
    for (const FComponentData& CompData : SceneData.Components)
    {
        UObject* NewCompObject = NewObject(CompData.Type);

        if (!NewCompObject)
        {
            UE_LOG("Failed to create Component: %s", CompData.Type.c_str());
            continue;
        }

        UActorComponent* NewActorComp = Cast<UActorComponent>(NewCompObject);
        if (!NewActorComp)
        {
            UE_LOG("Created object is not an ActorComponent: %s", CompData.Type.c_str());
            ObjectFactory::DeleteObject(NewCompObject);
            continue;
        }

        NewActorComp->UUID = CompData.UUID;

        // SceneComponent인 경우 Transform 설정
        if (USceneComponent* NewComp = Cast<USceneComponent>(NewActorComp))
        {
            NewComp->SetRelativeLocation(CompData.RelativeLocation);
            NewComp->SetRelativeRotation(FQuat::MakeFromEuler(CompData.RelativeRotation));
            NewComp->SetRelativeScale(CompData.RelativeScale);

            // Type별 속성 복원
            if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(NewComp))
            {
                if (!CompData.StaticMesh.empty())
                {
                    SMC->SetStaticMesh(CompData.StaticMesh);
                }
                // TODO: Materials 복원
            }
            else if (UDecalComponent* DecalComp = Cast<UDecalComponent>(NewComp))
            {
                // DecalComponent 속성 복원
                if (!CompData.DecalTexture.empty())
                {
                    DecalComp->SetDecalTexture(CompData.DecalTexture);
                }
                DecalComp->SetDecalSize(CompData.DecalSize);
                DecalComp->SetFadeInDuration(CompData.FadeInDuration);
                DecalComp->SetFadeStartDelay(CompData.FadeStartDelay);
                DecalComp->SetFadeDuration(CompData.FadeDuration);
                DecalComp->SetOrthoMatrixFlag(CompData.bIsOrthoMatrix);
            }
            else if (UBillboardComponent* BillboardComp = Cast<UBillboardComponent>(NewComp))
            {
                // BillboardComponent 속성 복원
                if (!CompData.BillboardTexturePath.empty())
                {
                    BillboardComp->SetTexture(CompData.BillboardTexturePath);
                }
                BillboardComp->SetBillboardSize(CompData.BillboardWidth, CompData.BillboardHeight);
                BillboardComp->SetUVCoords(CompData.UCoord, CompData.VCoord, CompData.ULength, CompData.VLength);
                BillboardComp->SetScreenSizeScaled(CompData.bIsScreenSizeScaled);
                BillboardComp->SetScreenSize(CompData.ScreenSize);
            }

            // Owner Actor 설정
            if (AActor** OwnerActor = ActorMap.Find(CompData.OwnerActorUUID))
            {
                NewComp->SetOwner(*OwnerActor);
            }

            ComponentMap.Add(CompData.UUID, NewComp);
        }
        // ActorComponent (Transform 없음)
        else
        {
            // MovementComponent 속성 복원
            if (UMovementComponent* MovementComp = Cast<UMovementComponent>(NewActorComp))
            {
                MovementComp->SetVelocity(CompData.Velocity);
                MovementComp->SetAcceleration(CompData.Acceleration);
                MovementComp->SetUpdateOnlyIfRendered(CompData.bUpdateOnlyIfRendered);

                // RotatingMovementComponent 추가 속성 복원
                if (URotatingMovementComponent* RotatingComp = Cast<URotatingMovementComponent>(MovementComp))
                {
                    RotatingComp->SetRotationRate(CompData.RotationRate);
                    RotatingComp->SetPivotTranslation(CompData.PivotTranslation);
                    RotatingComp->SetRotationInLocalSpace(CompData.bRotationInLocalSpace);
                }
                // ProjectileMovementComponent 추가 속성 복원
                else if (UProjectileMovementComponent* ProjectileComp = Cast<UProjectileMovementComponent>(MovementComp))
                {
                    ProjectileComp->SetGravity(CompData.Gravity);
                    ProjectileComp->SetInitialSpeed(CompData.InitialSpeed);
                    ProjectileComp->SetMaxSpeed(CompData.MaxSpeed);
                    ProjectileComp->SetHomingAccelerationMagnitude(CompData.HomingAccelerationMagnitude);
                    ProjectileComp->SetIsHomingProjectile(CompData.bIsHomingProjectile);
                    ProjectileComp->SetRotationFollowsVelocity(CompData.bRotationFollowsVelocity);
                    ProjectileComp->SetProjectileLifespan(CompData.ProjectileLifespan);
                    ProjectileComp->SetAutoDestroyWhenLifespanExceeded(CompData.bAutoDestroyWhenLifespanExceeded);
                    ProjectileComp->SetActive(CompData.bIsActive);
                }
            }

            // Owner Actor 설정
            if (AActor** OwnerActor = ActorMap.Find(CompData.OwnerActorUUID))
            {
                NewActorComp->SetOwner(*OwnerActor);
                // ActorComponent를 Actor의 OwnedComponents에 직접 추가
                (*OwnerActor)->OwnedComponents.Add(NewActorComp);
            }
        }
    }

    // ========================================
    // Pass 2: Actor-Component 연결 및 계층 구조 설정
    // ========================================
    for (const FActorData& ActorData : SceneData.Actors)
    {
        AActor** ActorPtr = ActorMap.Find(ActorData.UUID);
        if (!ActorPtr) continue;

        AActor* Actor = *ActorPtr;

        // RootComponent 설정
        if (USceneComponent** RootCompPtr = ComponentMap.Find(ActorData.RootComponentUUID))
        {
            Actor->RootComponent = *RootCompPtr;
        }
    }

    // Component 부모-자식 관계 설정
    for (const FComponentData& CompData : SceneData.Components)
    {
        USceneComponent** CompPtr = ComponentMap.Find(CompData.UUID);
        if (!CompPtr) continue;

        USceneComponent* Comp = *CompPtr;

        // 부모 컴포넌트 연결 (ParentUUID가 0이 아니면)
        if (CompData.ParentComponentUUID != 0)
        {
            if (USceneComponent** ParentPtr = ComponentMap.Find(CompData.ParentComponentUUID))
            {
                Comp->SetupAttachment(*ParentPtr, EAttachmentRule::KeepRelative);
            }
        }

        // Actor의 OwnedComponents에 추가
        if (AActor** OwnerActorPtr = ActorMap.Find(CompData.OwnerActorUUID))
        {
            (*OwnerActorPtr)->OwnedComponents.Add(Comp);
        }
    }

    // Actor를 Level에 추가
    for (auto& Pair : ActorMap)
    {
        AActor* Actor = Pair.second;
        Level->AddActor(Actor);

        // StaticMeshActor 전용 포인터 재설정
        if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
        {
            StaticMeshActor->SetStaticMeshComponent( Cast<UStaticMeshComponent>(StaticMeshActor->RootComponent));

            // CollisionComponent 찾기
            for (UActorComponent* Comp : StaticMeshActor->OwnedComponents)
            {
                if (UAABoundingBoxComponent* BBoxComp = Cast<UAABoundingBoxComponent>(Comp))
                {
                    StaticMeshActor->CollisionComponent = BBoxComp;
                    StaticMeshActor->SetCollisionComponent(EPrimitiveType::Sphere);
                    break;
                }
            }
        }
        // DecalActor 전용 포인터 재설정
        else if (ADecalActor* DecalActor = Cast<ADecalActor>(Actor))
        {
            // RootComponent를 DecalComponent로 재설정
            DecalActor->SetDecalComponent(Cast<UDecalComponent>(DecalActor->RootComponent));
        }

        // MovementComponent의 UpdatedComponent를 RootComponent로 설정
        for (UActorComponent* Comp : Actor->OwnedComponents)
        {
            if (UMovementComponent* MovementComp = Cast<UMovementComponent>(Comp))
            {
                MovementComp->SetUpdatedComponent(Actor->GetRootComponent());
            }
        }
    }

    // NextUUID 업데이트 (로드된 모든 UUID + 1)
    uint32 MaxUUID = SceneData.NextUUID;
    if (MaxUUID > UObject::PeekNextUUID())
    {
        UObject::SetNextUUID(MaxUUID);
    }

    BVH->Build(Level->GetActors());
    UE_LOG("Scene V2 loaded successfully: %s", SceneName.c_str());
}

AGizmoActor* UWorld::GetGizmoActor()
{
    return GizmoActor;
}

UWorld* UWorld::DuplicateWorldForPIE(UWorld* EditorWorld)
{
    if (!EditorWorld)
    {
        return nullptr;
    }

    // 새로운 PIE 월드 생성
    UWorld* PIEWorld = NewObject<UWorld>();
    if (!PIEWorld)
    {
        return nullptr;
    }
    PIEWorld->Renderer = EditorWorld->Renderer;
    PIEWorld->MainViewport = EditorWorld->MainViewport;
    PIEWorld->MultiViewport = EditorWorld->MultiViewport;
    // WorldType을 PIE로 설정
    PIEWorld->WorldType=(EWorldType::PIE);

    //// Renderer 공유 (얕은 복사)
    //PIEWorld->Renderer = EditorWorld->Renderer;

    // MainCameraActor 공유 (PIE는 일단 Editor 카메라 사용)
    PIEWorld->MainCameraActor = EditorWorld->MainCameraActor;

    // GizmoActor는 PIE에서 사용하지 않음
    PIEWorld->GizmoActor = nullptr;

    // GridActor 공유 (선택적)
    PIEWorld->GridActor = nullptr;

    // BVH 초기화 (PIE 월드용으로 새로 생성)
    PIEWorld->BVH = new FBVH();

    // Level 복제
    if (EditorWorld->GetLevel())
    {
        ULevel* EditorLevel = EditorWorld->GetLevel();
        ULevel* PIELevel = PIEWorld->GetLevel();

        if (PIELevel)
        {
            // Level의 Actors를 복제
            for (AActor* EditorActor : EditorLevel->GetActors())
            {
                if (EditorActor)
                {
                    // Duplication seed/context per-actor
                    TMap<UObject*, UObject*> DuplicationSeed;
                    TMap<UObject*, UObject*> CreatedObjects;

                    auto Params = InitStaticDuplicateObjectParams(
                        EditorActor,
                        PIEWorld,                 // DestOuter
                        FName::GetNone(),
                        DuplicationSeed,
                        CreatedObjects,
                        EDuplicateMode::PIE       // Duplicate mode for PIE
                    );

                    AActor* PIEActor = Cast<AActor>(EditorActor->Duplicate(Params));

                    if (PIEActor)
                    {
                        PIELevel->AddActor(PIEActor);
                        PIEActor->SetWorld(PIEWorld);
                    }
                }
            }

            PIEWorld->Level = PIELevel;
        }
    }
    return PIEWorld;
}

void UWorld::InitializeActorsForPlay()
{
    // PIE 월드의 BVH 빌드
    if (BVH && Level)
    {
        BVH->Build(Level->GetActors());
    }

    // 모든 액터의 BeginPlay 호출
    if (Level)
    {
        for (AActor* Actor : Level->GetActors())
        {
            if (Actor)
            {
                Actor->BeginPlay();
            }
        }
    }
}

void UWorld::CleanupWorld()
{
    if (Level)
    {
        for (AActor* Actor : Level->GetActors())
        {
            if (Actor)
            {
                Actor->EndPlay(EEndPlayReason::Quit);
            }
        }
    }
}

void UWorld::InitializeFullscreenQuad()
{
    SceneDepthShader = ResourceManager.Load<UShader>("SceneDepthShader.hlsl");
    if (!SceneDepthShader)
    {
        UE_LOG("ERROR: Failed to load SceneDepthShader.hlsl");
    }
}

void UWorld::RenderSceneDepthPass(const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix, FViewport* Viewport)
{
    if (!SceneDepthShader || !Renderer)
    {
        UE_LOG("ERROR: SceneDepthPass skipped - shader or renderer is null");
        return;
    }

    D3D11RHI* D3D11Device = static_cast<D3D11RHI*>(Renderer->GetRHIDevice());
    ID3D11DeviceContext* DeviceContext = D3D11Device->GetDeviceContext();

    // ============================================================
    // 0. 카메라 Near/Far Plane 및 Viewport 정보 가져오기
    // ============================================================
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;

    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* CameraComp = MainCameraActor->GetCameraComponent();
        NearPlane = CameraComp->GetNearClip();
        FarPlane = CameraComp->GetFarClip();
    }

    // ✅ Viewport 정보 가져오기
    float ViewportX = 0.0f;
    float ViewportY = 0.0f;
    float ViewportWidth = 1920.0f;  // 기본값
    float ViewportHeight = 1080.0f; // 기본값

    if (Viewport)
    {
        ViewportX = static_cast<float>(Viewport->GetStartX());
        ViewportY = static_cast<float>(Viewport->GetStartY());
        ViewportWidth = static_cast<float>(Viewport->GetSizeX());
        ViewportHeight = static_cast<float>(Viewport->GetSizeY());
    }

    // ✅ 전체 화면 크기 (메인 윈도우 크기)
    float ScreenWidth = CLIENTWIDTH;   // 외부에서 정의된 전역 변수
    float ScreenHeight = CLIENTHEIGHT; // 외부에서 정의된 전역 변수

    // ============================================================
    // 1. 현재 Viewport 정보 저장
    // ============================================================
    UINT NumViewports = 1;
    D3D11_VIEWPORT OldViewport;
    DeviceContext->RSGetViewports(&NumViewports, &OldViewport);

    // ============================================================
    // 2. Depth buffer를 SRV로 읽기 위해 DSV 언바인딩
    // ============================================================
    ID3D11RenderTargetView* pRTV = nullptr;
    ID3D11DepthStencilView* pDSV = nullptr;
    DeviceContext->OMGetRenderTargets(1, &pRTV, &pDSV);

    DeviceContext->OMSetRenderTargets(1, &pRTV, nullptr);

    if (pRTV) pRTV->Release();
    if (pDSV) pDSV->Release();

    // ============================================================
    // 3. 렌더링 상태 설정
    // ============================================================

    Renderer->OMSetDepthStencilState(EComparisonFunc::Always);

    // ✅ 메인 윈도우의 Depth SRV 사용 (전체 화면)
    ID3D11ShaderResourceView* DepthSRV = D3D11Device->GetDepthShaderResourceView();

    if (!DepthSRV)
    {
        UE_LOG("ERROR: DepthSRV is nullptr!");
        D3D11Device->OMSetRenderTargets();
        DeviceContext->RSSetViewports(1, &OldViewport);
        return;
    }

    DeviceContext->PSSetShaderResources(0, 1, &DepthSRV);

    Renderer->PrepareShader(SceneDepthShader);

    // ✅ Viewport 정보를 포함한 Constant Buffer 업데이트
    Renderer->UpdateDepthVisualizationBuffer(
        NearPlane, FarPlane,
        ViewportX, ViewportY,
        ViewportWidth, ViewportHeight,
        ScreenWidth, ScreenHeight
    );

    D3D11Device->PSSetDefaultSampler(0);

    // ============================================================
    // 4. Viewport 설정
    // ============================================================
    DeviceContext->RSSetViewports(1, &OldViewport);

    // ============================================================
    // 5. Fullscreen Triangle 렌더링
    // ============================================================

    DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    DeviceContext->Draw(3, 0);

    // ============================================================
    // 6. 정리 및 복원
    // ============================================================

    ID3D11ShaderResourceView* NullSRV = nullptr;
    DeviceContext->PSSetShaderResources(0, 1, &NullSRV);

    D3D11Device->OMSetRenderTargets();
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual);
    DeviceContext->RSSetViewports(1, &OldViewport);
}

/**
 * @brief 이미 생성한 Actor를 spawn하기 위한 shortcut 함수
 * @param InActor World에 생성할 Actor
 */
void UWorld::SpawnActor(AActor* InActor)
{
    InActor->SetWorld(this);


        if (UStaticMeshComponent* ActorComp = Cast<UStaticMeshComponent>(InActor->RootComponent))
        {
            FString ActorName = GenerateUniqueActorName(
                GetBaseNameNoExt(ActorComp->GetStaticMesh()->GetAssetPathFileName())
            );
            InActor->SetName(ActorName);
        }

    Level->GetActors().Add(InActor);

    // BVH 더티 플래그 설정
    MarkBVHDirty();
}

void UWorld::MarkBVHDirty()
{
    if (BVH)
    {
        BVH->MarkDirty();
    }
}

void UWorld::UpdateBVHIfNeeded()
{
    // BVH가 없으면 생성
    if (!BVH)
    {
        BVH = new FBVH();
    }

    if (!Level)
    {
        return;
    }

    bool bShouldRebuild = false;

    // 1. 더티 플래그 체크
    if (BVH->IsDirty())
    {
        bShouldRebuild = true;
    }

    // 2. 주기적 재빌드 체크 (BVHRebuildInterval > 0일 때만)
    if (BVHRebuildInterval > 0)
    {
        BVHFrameCounter++;
        if (BVHFrameCounter >= BVHRebuildInterval)
        {
            BVHFrameCounter = 0;
            bShouldRebuild = true;
        }
    }

    // 재빌드 수행
    if (bShouldRebuild)
    {
        BVH->Build(Level->GetActors()); // Rebuild 대신 Build 사용 (더티 플래그 체크 없이 무조건 빌드)
    }
}

void UWorld::PostProcessing()
{  
    if (!MultiViewport) return;

    // Store original viewport to restore it later
    D3D11_VIEWPORT OldViewport;
    UINT NumViewports = 1;
    Renderer->GetRHIDevice()->GetDeviceContext()->RSGetViewports(&NumViewports, &OldViewport);

    auto* Device = static_cast<D3D11RHI*>(Renderer->GetRHIDevice());

    auto ApplyFXAAOnVP = [&](FViewport* vp) {
        if (!vp) return;

        // Set the D3D viewport to match the logical viewport for this pass
        D3D11_VIEWPORT v{};
        v.TopLeftX = (float)vp->GetStartX();
        v.TopLeftY = (float)vp->GetStartY();
        v.Width = (float)vp->GetSizeX();
        v.Height = (float)vp->GetSizeY();
        v.MinDepth = 0.0f; v.MaxDepth = 1.0f;
        Renderer->GetRHIDevice()->GetDeviceContext()->RSSetViewports(1, &v);
           
        // Set the render target to the backbuffer for the final FXAA pass
        Device->OMSetBackBufferNoDepth();     
        
        ApplyFXAA(vp);
    };

    if (MultiViewport->GetCurrentLayoutMode() == EViewportLayoutMode::FourSplit) {
        auto** Viewports = MultiViewport->GetViewports();
        for (int i = 0; i < 4; ++i)
        { 
            ApplyFXAAOnVP(Viewports[i]->GetViewport());
        }
    }
    else 
    {
        FViewport* viewport = MultiViewport->GetMainViewport()->GetViewport();

        // Set the viewport for the Heat pass
        D3D11_VIEWPORT v{};
        v.TopLeftX = (float)viewport->GetStartX();
        v.TopLeftY = (float)viewport->GetStartY();
        v.Width = (float)viewport->GetSizeX();
        v.Height = (float)viewport->GetSizeY();
        v.MinDepth = 0.0f; v.MaxDepth = 1.0f;
        Renderer->GetRHIDevice()->GetDeviceContext()->RSSetViewports(1, &v);

        ApplyHeat(viewport);
        ApplyFXAAOnVP(viewport);
    } 

    // Restore the original viewport so UI and other rendering is not affected
    Renderer->GetRHIDevice()->GetDeviceContext()->RSSetViewports(1, &OldViewport);
}