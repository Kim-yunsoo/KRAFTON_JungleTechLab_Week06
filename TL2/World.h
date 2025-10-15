#pragma once
#include "GizmoActor.h"
#include "Object.h"
#include "GridActor.h"
#include "GizmoActor.h"
#include "Enums.h"
#include"Engine.h"
#include"Level.h"
#include"Frustum.h"

// Forward Declarations
class UResourceManager;
class UUIManager;
class UInputManager;
class USelectionManager;
class AActor;
class URenderer;
class ACameraActor;
class AGizmoActor;
class FViewport;
class SMultiViewportWindow;
struct FTransform;
struct FPrimitiveData;
class SViewportWindow;
class UOctree;
class FBVH;
class ULevel;

class FFrustum;
/**
 * UWorld
 * - 월드 단위의 액터/타임/매니저 관리 클래스
 */
class UWorld final : public UObject
{
public:
	DECLARE_CLASS(UWorld, UObject)
	UWorld();
	~UWorld() override;

public:
	/** 초기화 */
	void Initialize();
	void InitializeMainCamera();
	void InitializeGrid();
	void InitializeGizmo();
	void InitializeSceneGraph(TArray<AActor*>& Actors);

	void RenderSceneGraph();

	// 액터 인터페이스 관리
	void SetupActorReferences();

	// 선택 및 피킹 처리
	void ProcessActorSelection();

	void ProcessViewportInput();

	void SetRenderer(URenderer* InRenderer);
	URenderer* GetRenderer() { return Renderer; }

	void SetMainViewport(SViewportWindow* InViewport) { MainViewport = InViewport; }
	SViewportWindow* GetMainViewport() const { return MainViewport; }

	void SetMultiViewportWindow(SMultiViewportWindow* InMultiViewport) { MultiViewport = InMultiViewport; }
	SMultiViewportWindow* GetMultiViewportWindow() const { return MultiViewport; }

	template<class T>
	T* SpawnActor();

	template<class T>
	T* SpawnActor(const FTransform& Transform);

	void SpawnActor(AActor* InActor);

	bool DestroyActor(AActor* Actor);

	void CreateNewScene();
	// Version 1 (Legacy)
	void LoadScene(const FString& SceneName);
	void SaveScene(const FString& SceneName);

	// Version 2 (Component Hierarchy)
	void LoadSceneV2(const FString& SceneName);
	void SaveSceneV2(const FString& SceneName);
	ACameraActor* GetCameraActor() { return MainCameraActor; }

	EViewModeIndex GetViewModeIndex() { return ViewModeIndex; }
	void SetViewModeIndex(EViewModeIndex InViewModeIndex) { ViewModeIndex = InViewModeIndex; }

	/** Generate unique name for actor based on type */
	FString GenerateUniqueActorName(const FString& ActorType);

	/** === 타임 / 틱 === */
	virtual void Tick(float DeltaSeconds);
	float GetTimeSeconds() const;

	bool FrustumCullActors(const FFrustum& ViewFrustum, const AActor* Actor, int& FrustumCullCount);

	/** === 렌더 === */
	void Render();
	void RenderViewports(ACameraActor* Camera, FViewport* Viewport);
	void RenderEngineActors(const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix, FViewport* Viewport);
	//void GameRender(ACameraActor* Camera, FViewport* Viewport);

	//** === post processing == */
	void ApplyFXAA(FViewport* vt);
	void ApplyHeat(FViewport* vt);


	void PostProcessing();
	  /** === 필요한 엑터 게터 === */
	const TArray<AActor*>& GetActors() { return Level ? Level->GetActors() : Actors; }
	const TArray<AActor*>& GetEngineActors() const { return EngineActors; }
	AGizmoActor* GetGizmoActor();
	AGridActor* GetGridActor() { return GridActor; }

	UOctree* GetOctree() { return Octree; }
	FBVH* GetBVH() { return BVH; }

	// BVH 관리
	void MarkBVHDirty();
	void UpdateBVHIfNeeded();

	// BVH 주기적 재빌드 설정
	void SetBVHRebuildInterval(int32 FrameInterval) { BVHRebuildInterval = FrameInterval; }
	int32 GetBVHRebuildInterval() const { return BVHRebuildInterval; }

	ULevel* GetLevel() { return Level; };

	/** === 레벨 / 월드 구성 === */
	 //TArray<ULevel*> Levels;
	ULevel* Level;
	/** === 플레이어 / 컨트롤러 === */
	// APlayerController* GetFirstPlayerController() const;
	// TArray<APlayerController*> GetPlayerControllerIterator() const;

	/** === 에디터 월드 타입 === */
	EWorldType WorldType = EWorldType::Editor;//기본 설정은 에디터로 진행합니다.

	/** === PIE 관련 === */
	static UWorld* DuplicateWorldForPIE(UWorld* EditorWorld);
	void InitializeActorsForPlay();
	void CleanupWorld();

	bool IsPIEWorld() const { return WorldType == EWorldType::PIE; }

	// Fullscreen quad 초기화
	void InitializeFullscreenQuad();

	// Post-process pass 렌더링
	void RenderSceneDepthPass(const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix, FViewport* Viewport);

private:
	// 싱글톤 매니저 참조
	UResourceManager& ResourceManager;
	UUIManager& UIManager;
	UInputManager& InputManager;
	USelectionManager& SelectionManager;

	float TimeSeconds = 0.0f;


	// 메인 카메라
	ACameraActor* MainCameraActor = nullptr;

	AGridActor* GridActor = nullptr;
	// 렌더러 (월드가 소유)
	URenderer* Renderer;

	// 메인 뷰포트
	SViewportWindow* MainViewport = nullptr;
	// 멀티 뷰포트 윈도우
	SMultiViewportWindow* MultiViewport = nullptr;

	TArray<FPrimitiveData> Primitives;

	/** === 액터 관리 === */
	TArray<AActor*> EngineActors;
	/** === 액터 관리 === */
	TArray<AActor*> Actors;

	// Object naming system
	TMap<FString, int32> ObjectTypeCounts;

	/** == 기즈모 == */
	AGizmoActor* GizmoActor;

	EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Unlit;

	UOctree* Octree;
	FBVH* BVH;

	// BVH 주기적 재빌드 관련
	int32 BVHRebuildInterval = 30; // 0 = 더티 플래그만 사용, N = N프레임마다 재빌드
	int32 BVHFrameCounter = 0;

	// Scene Depth Pass용 전체 화면 쿼드 쉐이더
	UShader* SceneDepthShader = nullptr;
};
template<class T>
inline T* UWorld::SpawnActor()
{
	return SpawnActor<T>(FTransform());
}

template<class T>
inline T* UWorld::SpawnActor(const FTransform& Transform)
{
	static_assert(std::is_base_of<AActor, T>::value, "T must be derived from AActor");

	// 새 액터 생성
	T* NewActor = NewObject<T>();

	// 초기 트랜스폼 적용
	NewActor->SetActorTransform(Transform);

	//  월드 등록
	NewActor->SetWorld(this);

	if (Level)
	{
		Level->AddActor(NewActor);
	}

	// BVH 더티 플래그 설정
	MarkBVHDirty();

	return NewActor;
}
