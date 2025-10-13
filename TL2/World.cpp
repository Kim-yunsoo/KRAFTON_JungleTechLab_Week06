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
#include "LightComponent.h"
#include "MovementComponent.h"
#include "RotatingMovementComponent.h"

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
    // Level�� Actors ���� (PIE�� ������ ���͵鸸 ����)
    if (Level)
    {
        for (AActor* Actor : Level->GetActors())
        {
            ObjectFactory::DeleteObject(Actor);
        }

        // Level ��ü ����
        ObjectFactory::DeleteObject(Level);
        Level = nullptr;
    }

    // PIE ���尡 �ƴ� ���� ���� ���ҽ� ����
    if (WorldType == EWorldType::Editor)
    {
        // ī�޶� ����
        ObjectFactory::DeleteObject(MainCameraActor);
        MainCameraActor = nullptr;

        // Grid ����
        ObjectFactory::DeleteObject(GridActor);
        GridActor = nullptr;

        // GizmoActor ����
        ObjectFactory::DeleteObject(GizmoActor);
        GizmoActor = nullptr;

        // BVH ����
        if (BVH)
        {
            delete BVH;
            BVH = nullptr;
        }

        // ObjManager ����
        FObjManager::Clear();
    }
    else if (WorldType == EWorldType::PIE)
    {
        // PIE ������ BVH ���� (PIE �������� ���� ���������Ƿ� ���� �ʿ�)
        if (BVH)
        {
            delete BVH;
            BVH = nullptr;
        }

        // PIE ����� ���� �����͸� nullptr�� ���� (�������� ����)
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

    // 1) ���� ���� Ÿ�� �̸�
    std::snprintf(buf, sizeof(buf), "[RTTI] TypeName = %s\r\n", Obj->GetClass()->Name);
    UE_LOG(buf);

    // 2) IsA üũ (�Ļ� ����)
    std::snprintf(buf, sizeof(buf), "[RTTI] IsA<AActor>      = %d\r\n", (int)Obj->IsA<AActor>());
    UE_LOG(buf);
    std::snprintf(buf, sizeof(buf), "[RTTI] IsA<ACameraActor> = %d\r\n",
                  (int)Obj->IsA<ACameraActor>());
    UE_LOG(buf);

    //// 3) ��Ȯ�� Ÿ�� �� (�Ļ� ����)
    //std::snprintf(buf, sizeof(buf), "[RTTI] EXACT ACameraActor = %d\r\n",
    //    (int)(Obj->GetClass() == ACameraActor::StaticClass()));
    //UE_LOG(buf);

    // 4) ��� ü�� ���
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

    // �� �� ����
    CreateNewScene();

    InitializeMainCamera();
    InitializeGrid();
    InitializeGizmo();

    // BVH �ʱ�ȭ (�� ���·� ����)
    if (!BVH)
    {
        BVH = new FBVH();
    }

    // ���� �� ���� ����
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
    // === ����� ���� �ʱ�ȭ ===
    GizmoActor = NewObject<AGizmoActor>();
    GizmoActor->SetWorld(this);
    GizmoActor->SetActorTransform(FTransform(FVector{0, 0, 0},
                                             FQuat::MakeFromEuler(FVector{0, -90, 0}),
                                             FVector{1, 1, 1}));
    // ����� ī�޶� ���� ����
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

    // ���� �Ϸ� �� ��� ����ũ�� BVH �̸� ����
    Octree->PreBuildAllMicroBVH();

    // BVH �ʱ�ȭ �� ����
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

    // UIManager�� ����Ʈ ��ȯ ���¿� ���� ������ ���� SWidget���� �������ٰ���

    if (MultiViewport)
    {
        MultiViewport->OnRender();
    }

    //������ ���� 
    Renderer->EndFrame();
    UIManager.EndFrame();
    Renderer->GetRHIDevice()->Present();
}

void UWorld::RenderViewports(ACameraActor* Camera, FViewport* Viewport)
{
    // ����Ʈ�� ���� ũ��� aspect ratio ���
    float ViewportAspectRatio = static_cast<float>(Viewport->GetSizeX()) / static_cast<float>(
        Viewport->GetSizeY());
    if (Viewport->GetSizeY() == 0)
    {
        ViewportAspectRatio = 1.0f;
    } // 0���� ������ ����

    FMatrix ViewMatrix = Camera->GetViewMatrix();
    FMatrix ProjectionMatrix = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);
    if (!Renderer)
    {
        return;
    }

    // Pass 0: Visible Lights �� Pruning�ϴ� ����
    {
        TArray<FLightInfo> VisibleFrameLights;

        const TArray<AActor*>& Actors = Level ? Level->GetActors() : TArray<AActor*>();

        for (AActor* Actor : Actors)
        {
            if (!Actor || Actor->GetActorHiddenInGame()) continue;

            for (UActorComponent* Comp : Actor->GetComponents())
            {
                USceneComponent* SceneComp = Cast<USceneComponent>(Comp);
                if (SceneComp == nullptr) continue;

                if (ULightComponent* LightComp = Cast<ULightComponent>(SceneComp))
                {
                    FLightInfo LightInfo;
                    LightInfo.Type = ELighType::Spot;

                    LightInfo.LightPos = SceneComp->GetWorldLocation();
                    LightInfo.Radius = 10.0f;
                    LightInfo.RadiusFallOff = 1.0f;
                    LightInfo.Color = FVector4(1, 0, 0, 1);
                    LightInfo.Intensity = 1.0f;
                    //TODO: Light Dir
                    if (VisibleFrameLights.size() < 8)
                    {
                        VisibleFrameLights.Add(LightInfo);
                    }
                }
            }
        }

        Renderer->SetWorldLights(VisibleFrameLights);
        Renderer->UpdateLightBuffer();
    }

    FVector rgb(1.0f, 1.0f, 1.0f);

    FFrustum ViewFrustum;
    ViewFrustum.Update(ViewMatrix * ProjectionMatrix);

    Renderer->BeginLineBatch();
    Renderer->SetViewModeType(ViewModeIndex);

    int AllActorCount = 0;
    int FrustumCullCount = 0;
    int32 TotalDecalCount = 0;

    const TArray<AActor*>& LevelActors = Level ? Level->GetActors() : TArray<AActor*>();

	// Pass 1: ��Į�� ������ ��� ������Ʈ ������ (Depth ���� ä���)
	for (AActor* Actor : LevelActors)
	{
		// �Ϲ� ���͵� ������
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

			// Decal Component�� ��� Editor Visuals�� ������ (���� ��Į ������ �н� 2����)
            if (UDecalComponent* DecalComp = Cast<UDecalComponent>(Component))
            {
                DecalComp->RenderEditorVisuals(Renderer, ViewMatrix, ProjectionMatrix);
				TotalDecalCount++;
                continue;
            }

			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				bool bIsSelected = SelectionManager.IsActorSelected(Actor);

				//// ���õ� ���ʹ� �׻� �տ� ���̵��� depth test�� Always�� ����
				//if (bIsSelected)//���߿� �߰�����
				//{
				//    Renderer->OMSetDepthStencilState(EComparisonFunc::Always);
				//}

				Renderer->UpdateHighLightConstantBuffer(bIsSelected, rgb, 0, 0, 0, 0);
				Primitive->Render(Renderer, ViewMatrix, ProjectionMatrix, Viewport);

				//// depth test ������� ����
				//if (bIsSelected)
				//{
				//    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual);
				//}
			}
		}
		Renderer->OMSetBlendState(false);
	}

    // ���� ���͵� (�׸��� ��) ������
    RenderEngineActors(ViewMatrix, ProjectionMatrix, Viewport);

    URenderingStatsCollector& StatsCollector = URenderingStatsCollector::GetInstance();
    
    // Pass 2: ��Į ������
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

	Renderer->EndLineBatch(FMatrix::Identity(), ViewMatrix, ProjectionMatrix);
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

void UWorld::Tick(float DeltaSeconds)
{
    // Level�� Actors Tick
    if (Level)
    {
        for (AActor* Actor : Level->GetActors())
        {
            if (Actor && Actor->IsActorTickEnabled())
            {
               Actor->Tick(DeltaSeconds);
            }

            // Actor�� Tick�� ���� �Ŀ� 
            // Decal�� �浹�� Actor�� �����Ѵ�. 
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
    //Input Manager�� ī�޶� �Ŀ� ������Ʈ �Ǿ����

    // ����Ʈ ������Ʈ - UIManager�� ����Ʈ ��ȯ ���¿� ����
    if (MultiViewport)
    {
        MultiViewport->OnUpdate(DeltaSeconds);
    }

    //InputManager.Update();
    UIManager.Update(DeltaSeconds);

    // BVH ������Ʈ (Transform ������ ���� ���)
    UpdateBVHIfNeeded();
}

float UWorld::GetTimeSeconds() const
{
    return 0.0f;
}

bool UWorld::FrustumCullActors(const FFrustum& ViewFrustum, const AActor* Actor, int & FrustumCullCount)
{
    if (Actor->CollisionComponent)
    {
        FBound Test = Actor->CollisionComponent->GetWorldBoundFromCube();

        // ����ü �ۿ� �ִٸ�, �� ������ ������ ������ ��� �ǳʶݴϴ�.
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
// ���� ����
//
bool UWorld::DestroyActor(AActor* Actor)
{
    if (!Actor)
    {
        return false; // nullptr ���� �� ����
    }

    // SelectionManager���� ���� ���� (�޸� ���� ���� ����)
    USelectionManager::GetInstance().DeselectActor(Actor);

    // UIManager���� �ȵ� ���� ����
    if (UIManager.GetPickedActor() == Actor)
    {
        UIManager.ResetPickedActor();
    }

    // �迭����  ���� �õ�
    // Level���� ���� �õ�
    if (Level)
    {
        Level->RemoveActor(Actor);

        // �޸� ����
        ObjectFactory::DeleteObject(Actor);
        // ������ ���� ����
        USelectionManager::GetInstance().CleanupInvalidActors();

        // BVH ��Ƽ �÷��� ����
        MarkBVHDirty();

        return true; // ���������� ����
    }

    return false; // ���忡 ���� ����
}

inline FString ToObjFileName(const FString& TypeName)
{
    return "Data/" + TypeName + ".obj";
}

inline FString RemoveObjExtension(const FString& FileName)
{
    const FString Extension = ".obj";

    // ������ ��� ������ ��ġ Ž�� (POSIX/Windows ��� ����)
    const uint64 Sep = FileName.find_last_of("/\\");
    const uint64 Start = (Sep == FString::npos) ? 0 : Sep + 1;

    // Ȯ���� ���� ��ġ ����
    uint64 End = FileName.size();
    if (End >= Extension.size() &&
        FileName.compare(End - Extension.size(), Extension.size(), Extension) == 0)
    {
        End -= Extension.size();
    }

    // ���̽� �̸�(Ȯ���� ���� ���ϸ�) ��ȯ
    if (Start <= End)
    {
        return FileName.substr(Start, End - Start);
    }

    // ������ �Է� �� ���� ��ȯ (������ġ)
    return FileName;
}

void UWorld::CreateNewScene()
{
    // Safety: clear interactions that may hold stale pointers
    SelectionManager.ClearSelection();
    UIManager.ResetPickedActor();
    // Level�� Actors ����
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
        Octree->Release();//���ο� ���� ����� Octree�� �����ش�.
    }
    if (BVH)
    {
        BVH->Clear();//���ο� ���� ����� BVH�� �����ش�.
    }
    // �̸� ī���� �ʱ�ȭ: ���� ���� ������ �� �� BaseName �� suffix�� 0���� �ٽ� ����
    ObjectTypeCounts.clear();
}

// ���� �������̽� ���� �޼ҵ��
void UWorld::SetupActorReferences()
{
    /*if (GizmoActor && MainCameraActor)
    {
        GizmoActor->SetCameraActor(MainCameraActor);
    }*/
}

//���콺 ��ŷ���� �޼ҵ�
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

    // [1] �ε� ���� �� ���� ī���� ���
    const uint32 PreLoadNext = UObject::PeekNextUUID();

    // [2] ���� NextUUID�� ���纸�� Ŭ ���� �ݿ�(���� ���� ���� ����)
    uint32 LoadedNextUUID = 0;
    if (FSceneLoader::TryReadNextUUID(FilePath, LoadedNextUUID))
    {
        if (LoadedNextUUID > UObject::PeekNextUUID())
        {
            UObject::SetNextUUID(LoadedNextUUID);
        }
    }

    // [3] ���� �� ����
    CreateNewScene();

    // [4] �ε�
    FPerspectiveCameraData CamData{};
    const TArray<FPrimitiveData>& Primitives = FSceneLoader::Load(FilePath, &CamData);

    // ���콺 ��Ÿ �ʱ�ȭ
    const FVector2D CurrentMousePos = UInputManager::GetInstance().GetMousePosition();
    UInputManager::GetInstance().SetLastMousePosition(CurrentMousePos);

    // ī�޶� ����
    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();

        // ��ġ/ȸ��(���� Ʈ������)
        MainCameraActor->SetActorLocation(CamData.Location);
        MainCameraActor->SetActorRotation(FQuat::MakeFromEuler(CamData.Rotation));

        // �Է� ��ο� ������ ������� ����/ȸ�� ����
        // ����: Pitch = CamData.Rotation.Y, Yaw = CamData.Rotation.Z
        MainCameraActor->SetAnglesImmediate(CamData.Rotation.Y, CamData.Rotation.Z);

        // UIManager�� ī�޶� ȸ�� ���µ� ����ȭ
        UIManager.UpdateMouseRotation(CamData.Rotation.Y, CamData.Rotation.Z);

        // �������� �Ķ����
        Cam->SetFOV(CamData.FOV);
        Cam->SetClipPlanes(CamData.NearClip, CamData.FarClip);

        // UI ������ ���� ī�޶� ���·� �絿��ȭ ��û
        UIManager.SyncCameraControlFromCamera();
    }

    // 1) ���� ���忡�� �̹� ��� ���� UUID ����(���� ���� + �����)
    std::unordered_set<uint32> UsedUUIDs;
    auto AddUUID = [&](AActor* A) { if (A) UsedUUIDs.insert(A->UUID); };
    for (AActor* Eng : EngineActors)
    {
        AddUUID(Eng);
    }
    AddUUID(GizmoActor); // Gizmo�� EngineActors�� �� �� �� �����Ƿ� ��� �߰�

    uint32 MaxAssignedUUID = 0;

    for (const FPrimitiveData& Primitive : Primitives)
    {
        // ���� �� �ʿ��� �ʱ� Ʈ�������� �״�� �ѱ�
        AStaticMeshActor* StaticMeshActor = SpawnActor<AStaticMeshActor>(
            FTransform(Primitive.Location,
                       SceneRotUtil::QuatFromEulerZYX_Deg(Primitive.Rotation),
                       Primitive.Scale));

        // ���� ������ �ڵ� �߱޵� ���� UUID (�浹 �� �������� ���)
        uint32 Assigned = StaticMeshActor->UUID;

        // �켱 ������ UUID�� ��� ������ ���
        UsedUUIDs.insert(Assigned);

        // 2) ������ UUID�� �켱 �����ϵ�, �浹�̸� ������ UUID ����
        if (Primitive.UUID != 0)
        {
            if (UsedUUIDs.find(Primitive.UUID) == UsedUUIDs.end())
            {
                // ������ ID ��� ���� �� ��ü
                UsedUUIDs.erase(Assigned);
                StaticMeshActor->UUID = Primitive.UUID;
                Assigned = Primitive.UUID;
                UsedUUIDs.insert(Assigned);
            }
            else
            {
                // �浹: ���� UUID ��� �Ұ� �� ��� �α� �� ������ ���� UUID ����
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

 

    // 3) ���� ����: ���� ī���ʹ� ���� ���� ���� + ���� ���� �ִ밪 ���ķ� ����
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
                SMC->Serialize(false, Data); // ���⼭ RotUtil �����(���� Serialize)
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
                Prim->Serialize(false, Data); // ���⼭ RotUtil �����
            }
            else
            {
                // ��Ʈ�� Primitive�� �ƴ� ���� ���� ��Ģ���� ����
                Data.Location = Actor->GetActorLocation();
                Data.Rotation = SceneRotUtil::EulerZYX_Deg_FromQuat(Actor->GetActorRotation());
                Data.Scale = Actor->GetActorScale();
            }

            Data.ObjStaticMeshAsset.clear();
            Primitives.push_back(Data);
        }
    }

    // ī�޶� ������ ä���
    const FPerspectiveCameraData* CamPtr = nullptr;
    FPerspectiveCameraData CamData;
    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();

        CamData.Location = MainCameraActor->GetActorLocation();

        // ���� ���� ������ ����: Pitch=Y, Yaw=Z, Roll=0
        CamData.Rotation.X = 0.0f;
        CamData.Rotation.Y = MainCameraActor->GetCameraPitch();
        CamData.Rotation.Z = MainCameraActor->GetCameraYaw();

        CamData.FOV = Cam->GetFOV();
        CamData.NearClip = Cam->GetNearClip();
        CamData.FarClip = Cam->GetFarClip();
        CamPtr = &CamData;
    }

    // Scene ���͸��� ����
    FSceneLoader::Save(Primitives, CamPtr, SceneName);
}

void UWorld::SaveSceneV2(const FString& SceneName)
{
    FSceneData SceneData;
    SceneData.Version = 2;
    SceneData.NextUUID = UObject::PeekNextUUID();

    // ī�޶� ������ ä���
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

    // Actor �� Component ���� ����
    for (AActor* Actor : Level->GetActors())
    {
        if (!Actor) continue;

        // Actor ������
        FActorData ActorData;
        ActorData.UUID = Actor->UUID;
        ActorData.Name = Actor->GetName().ToString();
        ActorData.Type = Actor->GetClass()->Name;

        if (Actor->GetRootComponent())
            ActorData.RootComponentUUID = Actor->GetRootComponent()->UUID;

        SceneData.Actors.push_back(ActorData);

        // OwnedComponents ��ȸ (��� ������Ʈ ����)
        for (UActorComponent* ActorComp : Actor->GetComponents())
        {
            if (!ActorComp) continue;

            FComponentData CompData;
            CompData.UUID = ActorComp->UUID;
            CompData.OwnerActorUUID = Actor->UUID;
            CompData.Type = ActorComp->GetClass()->Name;

            // SceneComponent�� ��� Transform�� ���� ���� ���� ����
            if (USceneComponent* Comp = Cast<USceneComponent>(ActorComp))
            {
                // �θ� ������Ʈ UUID (RootComponent�� 0)
                if (Comp->GetAttachParent())
                    CompData.ParentComponentUUID = Comp->GetAttachParent()->UUID;
                else
                    CompData.ParentComponentUUID = 0;

                // Transform
                CompData.RelativeLocation = Comp->GetRelativeLocation();
                CompData.RelativeRotation = Comp->GetRelativeRotation().ToEuler();
                CompData.RelativeScale = Comp->GetRelativeScale();

                // Type�� �Ӽ�
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
                    // TODO: Materials ����
                }
                else if (UDecalComponent* DecalComp = Cast<UDecalComponent>(Comp))
                {
                    // DecalComponent �Ӽ� ����
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
                    // BillboardComponent �Ӽ� ����
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
                // ActorComponent (Transform ����)
                CompData.ParentComponentUUID = 0;

                // MovementComponent �Ӽ� ����
                if (UMovementComponent* MovementComp = Cast<UMovementComponent>(ActorComp))
                {
                    CompData.Velocity = MovementComp->GetVelocity();
                    CompData.Acceleration = MovementComp->GetAcceleration();
                    CompData.bUpdateOnlyIfRendered = MovementComp->GetUpdateOnlyIfRendered();

                    // RotatingMovementComponent �߰� �Ӽ� ����
                    if (URotatingMovementComponent* RotatingComp = Cast<URotatingMovementComponent>(MovementComp))
                    {
                        CompData.RotationRate = RotatingComp->GetRotationRate();
                        CompData.PivotTranslation = RotatingComp->GetPivotTranslation();
                        CompData.bRotationInLocalSpace = RotatingComp->IsRotationInLocalSpace();
                    }
                }
            }

            SceneData.Components.push_back(CompData);
        }
    }

    // Scene ���͸��� V2 �������� ����
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

    // NextUUID ������Ʈ
    uint32 LoadedNextUUID = 0;
    if (FSceneLoader::TryReadNextUUID(FilePath, LoadedNextUUID))
    {
        if (LoadedNextUUID > UObject::PeekNextUUID())
        {
            UObject::SetNextUUID(LoadedNextUUID);
        }
    }

    // ���� �� ����
    CreateNewScene();

    // V2 ������ �ε�
    FSceneData SceneData = FSceneLoader::LoadV2(FilePath);

    // ���콺 ��Ÿ �ʱ�ȭ
    const FVector2D CurrentMousePos = UInputManager::GetInstance().GetMousePosition();
    UInputManager::GetInstance().SetLastMousePosition(CurrentMousePos);

    if (MainCameraActor && MainCameraActor->GetCameraComponent())
    {
        UCameraComponent* Cam = MainCameraActor->GetCameraComponent();
        MainCameraActor->SetActorLocation(SceneData.Camera.Location);
        MainCameraActor->SetCameraPitch(SceneData.Camera.Rotation.Y);
        MainCameraActor->SetCameraYaw(SceneData.Camera.Rotation.Z);

        // �Է� ��ο� ������ ������� ����/ȸ�� ����
      // ����: Pitch = CamData.Rotation.Y, Yaw = CamData.Rotation.Z
        MainCameraActor->SetAnglesImmediate(SceneData.Camera.Rotation.Y, SceneData.Camera.Rotation.Z);

        // UIManager�� ī�޶� ȸ�� ���µ� ����ȭ
        UIManager.UpdateMouseRotation(SceneData.Camera.Rotation.Y, SceneData.Camera.Rotation.Z);

        Cam->SetFOV(SceneData.Camera.FOV);
        Cam->SetClipPlanes(SceneData.Camera.NearClip, SceneData.Camera.FarClip);

        // UI ������ ���� ī�޶� ���·� �絿��ȭ ��û
        UIManager.SyncCameraControlFromCamera();
      
    }

    // UUID �� Object ���� ���̺�
    TMap<uint32, AActor*> ActorMap;
    TMap<uint32, USceneComponent*> ComponentMap;

    // ========================================
    // Pass 1: Actor �� Component ����
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

        // DecalActor�� ��� �����ڰ� ���� DecalComponent�� ����
        if (ADecalActor* DecalActor = Cast<ADecalActor>(NewActor))
        {
            DecalActor->ClearDefaultComponents();
        }
        // StaticMeshActor�� ��� �����ڰ� ���� ������Ʈ���� ����
        else if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(NewActor))
        {
            StaticMeshActor->ClearDefaultComponents();
        }

        ActorMap.Add(ActorData.UUID, NewActor);
    }

    // Component ����
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

        // SceneComponent�� ��� Transform ����
        if (USceneComponent* NewComp = Cast<USceneComponent>(NewActorComp))
        {
            NewComp->SetRelativeLocation(CompData.RelativeLocation);
            NewComp->SetRelativeRotation(FQuat::MakeFromEuler(CompData.RelativeRotation));
            NewComp->SetRelativeScale(CompData.RelativeScale);

            // Type�� �Ӽ� ����
            if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(NewComp))
            {
                if (!CompData.StaticMesh.empty())
                {
                    SMC->SetStaticMesh(CompData.StaticMesh);
                }
                // TODO: Materials ����
            }
            else if (UDecalComponent* DecalComp = Cast<UDecalComponent>(NewComp))
            {
                // DecalComponent �Ӽ� ����
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
                // BillboardComponent �Ӽ� ����
                if (!CompData.BillboardTexturePath.empty())
                {
                    BillboardComp->SetTexture(CompData.BillboardTexturePath);
                }
                BillboardComp->SetBillboardSize(CompData.BillboardWidth, CompData.BillboardHeight);
                BillboardComp->SetUVCoords(CompData.UCoord, CompData.VCoord, CompData.ULength, CompData.VLength);
                BillboardComp->SetScreenSizeScaled(CompData.bIsScreenSizeScaled);
                BillboardComp->SetScreenSize(CompData.ScreenSize);
            }

            // Owner Actor ����
            if (AActor** OwnerActor = ActorMap.Find(CompData.OwnerActorUUID))
            {
                NewComp->SetOwner(*OwnerActor);
            }

            ComponentMap.Add(CompData.UUID, NewComp);
        }
        // ActorComponent (Transform ����)
        else
        {
            // MovementComponent �Ӽ� ����
            if (UMovementComponent* MovementComp = Cast<UMovementComponent>(NewActorComp))
            {
                MovementComp->SetVelocity(CompData.Velocity);
                MovementComp->SetAcceleration(CompData.Acceleration);
                MovementComp->SetUpdateOnlyIfRendered(CompData.bUpdateOnlyIfRendered);

                // RotatingMovementComponent �߰� �Ӽ� ����
                if (URotatingMovementComponent* RotatingComp = Cast<URotatingMovementComponent>(MovementComp))
                {
                    RotatingComp->SetRotationRate(CompData.RotationRate);
                    RotatingComp->SetPivotTranslation(CompData.PivotTranslation);
                    RotatingComp->SetRotationInLocalSpace(CompData.bRotationInLocalSpace);
                }
            }

            // Owner Actor ����
            if (AActor** OwnerActor = ActorMap.Find(CompData.OwnerActorUUID))
            {
                NewActorComp->SetOwner(*OwnerActor);
                // ActorComponent�� Actor�� OwnedComponents�� ���� �߰�
                (*OwnerActor)->OwnedComponents.Add(NewActorComp);
            }
        }
    }

    // ========================================
    // Pass 2: Actor-Component ���� �� ���� ���� ����
    // ========================================
    for (const FActorData& ActorData : SceneData.Actors)
    {
        AActor** ActorPtr = ActorMap.Find(ActorData.UUID);
        if (!ActorPtr) continue;

        AActor* Actor = *ActorPtr;

        // RootComponent ����
        if (USceneComponent** RootCompPtr = ComponentMap.Find(ActorData.RootComponentUUID))
        {
            Actor->RootComponent = *RootCompPtr;
        }
    }

    // Component �θ�-�ڽ� ���� ����
    for (const FComponentData& CompData : SceneData.Components)
    {
        USceneComponent** CompPtr = ComponentMap.Find(CompData.UUID);
        if (!CompPtr) continue;

        USceneComponent* Comp = *CompPtr;

        // �θ� ������Ʈ ���� (ParentUUID�� 0�� �ƴϸ�)
        if (CompData.ParentComponentUUID != 0)
        {
            if (USceneComponent** ParentPtr = ComponentMap.Find(CompData.ParentComponentUUID))
            {
                Comp->SetupAttachment(*ParentPtr, EAttachmentRule::KeepRelative);
            }
        }

        // Actor�� OwnedComponents�� �߰�
        if (AActor** OwnerActorPtr = ActorMap.Find(CompData.OwnerActorUUID))
        {
            (*OwnerActorPtr)->OwnedComponents.Add(Comp);
        }
    }

    // Actor�� Level�� �߰�
    for (auto& Pair : ActorMap)
    {
        AActor* Actor = Pair.second;
        Level->AddActor(Actor);

        // StaticMeshActor ���� ������ �缳��
        if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
        {
            StaticMeshActor->SetStaticMeshComponent( Cast<UStaticMeshComponent>(StaticMeshActor->RootComponent));

            // CollisionComponent ã��
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
        // DecalActor ���� ������ �缳��
        else if (ADecalActor* DecalActor = Cast<ADecalActor>(Actor))
        {
            // RootComponent�� DecalComponent�� �缳��
            DecalActor->SetDecalComponent(Cast<UDecalComponent>(DecalActor->RootComponent));
        }

        // MovementComponent�� UpdatedComponent�� RootComponent�� ����
        for (UActorComponent* Comp : Actor->OwnedComponents)
        {
            if (UMovementComponent* MovementComp = Cast<UMovementComponent>(Comp))
            {
                MovementComp->SetUpdatedComponent(Actor->GetRootComponent());
            }
        }
    }

    // NextUUID ������Ʈ (�ε�� ��� UUID + 1)
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

    // ���ο� PIE ���� ����
    UWorld* PIEWorld = NewObject<UWorld>();
    if (!PIEWorld)
    {
        return nullptr;
    }
    PIEWorld->Renderer = EditorWorld->Renderer;
    PIEWorld->MainViewport = EditorWorld->MainViewport;
    PIEWorld->MultiViewport = EditorWorld->MultiViewport;
    // WorldType�� PIE�� ����
    PIEWorld->WorldType=(EWorldType::PIE);

    //// Renderer ���� (���� ����)
    //PIEWorld->Renderer = EditorWorld->Renderer;

    // MainCameraActor ���� (PIE�� �ϴ� Editor ī�޶� ���)
    PIEWorld->MainCameraActor = EditorWorld->MainCameraActor;

    // GizmoActor�� PIE���� ������� ����
    PIEWorld->GizmoActor = nullptr;

    // GridActor ���� (������)
    PIEWorld->GridActor = nullptr;

    // BVH �ʱ�ȭ (PIE ��������� ���� ����)
    PIEWorld->BVH = new FBVH();

    // Level ����
    if (EditorWorld->GetLevel())
    {
        ULevel* EditorLevel = EditorWorld->GetLevel();
        ULevel* PIELevel = PIEWorld->GetLevel();

        if (PIELevel)
        {
            // Level�� Actors�� ����
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
    // PIE ������ BVH ����
    if (BVH && Level)
    {
        BVH->Build(Level->GetActors());
    }

    // ��� ������ BeginPlay ȣ��
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

/**
 * @brief �̹� ������ Actor�� spawn�ϱ� ���� shortcut �Լ�
 * @param InActor World�� ������ Actor
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

    // BVH ��Ƽ �÷��� ����
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
    // BVH�� ������ ����
    if (!BVH)
    {
        BVH = new FBVH();
    }

    if (!Level)
    {
        return;
    }

    bool bShouldRebuild = false;

    // 1. ��Ƽ �÷��� üũ
    if (BVH->IsDirty())
    {
        bShouldRebuild = true;
    }

    // 2. �ֱ��� ����� üũ (BVHRebuildInterval > 0�� ����)
    if (BVHRebuildInterval > 0)
    {
        BVHFrameCounter++;
        if (BVHFrameCounter >= BVHRebuildInterval)
        {
            BVHFrameCounter = 0;
            bShouldRebuild = true;
        }
    }

    // ����� ����
    if (bShouldRebuild)
    {
        BVH->Build(Level->GetActors()); // Rebuild ��� Build ��� (��Ƽ �÷��� üũ ���� ������ ����)
    }
}

