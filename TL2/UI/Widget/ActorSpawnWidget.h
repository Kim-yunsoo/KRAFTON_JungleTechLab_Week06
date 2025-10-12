#pragma once
#include "Widget.h"
#include "../../Vector.h"

class UUIManager;
class UWorld;

/**
 * @brief 통합 Actor Spawn Widget
 * Empty Actor, StaticMeshActor, DecalActor를 선택적으로 스폰할 수 있습니다.
 */
class UActorSpawnWidget : public UWidget
{
public:
	DECLARE_CLASS(UActorSpawnWidget, UWidget)

	void Initialize() override;
	void Update() override;
	void RenderWidget() override;
	void SpawnActors() const;

	// Special Member Function
	UActorSpawnWidget();
	~UActorSpawnWidget() override;

private:
	UUIManager* UIManager = nullptr;

	// Actor 타입 선택
	enum class EActorType : int32
	{
		Empty = 0,
		StaticMesh = 1,
		Decal = 2,
		PerspectiveDecal = 3,
	};

	int32 SelectedActorType = static_cast<int32>(EActorType::Empty);

	// Spawn 설정
	int32 NumberOfSpawn = 1;
	float SpawnRangeMin = 0.0f;
	float SpawnRangeMax = 0.0f;
	bool bRandomRotation = false;
	bool bRandomScale = false;
	float MinScale = 0.5f;
	float MaxScale = 2.0f;

	// StaticMeshActor 전용
	mutable int32 SelectedMeshIndex = -1;
	mutable TArray<FString> CachedMeshFilePaths;

	// DecalActor 전용
	mutable int32 SelectedDecalTextureIndex = -1;
	mutable TArray<FString> CachedDecalTexturePaths;

	// Fade 설정
	float FadeInDuration = 1.0f;
	float FadeStartDelay = 3.0f;
	float FadeOutDuration = 1.0f;
	float MaxAlpha = 1.0f;

	// 헬퍼 메서드
	UWorld* GetCurrentWorld() const;
	FVector GenerateRandomLocation() const;
	float GenerateRandomScale() const;
	FQuat GenerateRandomRotation() const;
	const char* GetActorTypeName(int32 TypeIndex) const;
	TArray<FString> GetDecalFiles() const;

	void SpawnEmptyActor(UWorld* World) const;
	void SpawnStaticMeshActor(UWorld* World) const;
	void SpawnDecalActor(UWorld* World, bool bIsOrtho) const;
};
