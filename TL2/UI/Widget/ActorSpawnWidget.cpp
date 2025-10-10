#include "pch.h"
#include "ActorSpawnWidget.h"
#include "../UIManager.h"
#include "../../ImGui/imgui.h"
#include "../../World.h"
#include "../../Actor.h"
#include "../../StaticMeshActor.h"
#include "../../DecalActor.h"
#include "../../DecalComponent.h"
#include "../../Vector.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>

using std::max;
using std::min;

static inline FString GetBaseNameNoExt(const FString& Path)
{
	const size_t Sep = Path.find_last_of("/\\");
	const size_t Start = (Sep == FString::npos) ? 0 : Sep + 1;

	const FString Ext1 = ".obj";
	const FString Ext2 = ".dds";
	const FString Ext3 = ".jpg";
	const FString Ext4 = ".png";

	size_t End = Path.size();

	// 확장자 제거
	for (const FString& Ext : {Ext1, Ext2, Ext3, Ext4})
	{
		if (End >= Ext.size() && Path.compare(End - Ext.size(), Ext.size(), Ext) == 0)
		{
			End -= Ext.size();
			break;
		}
	}

	if (Start <= End)
	{
		return Path.substr(Start, End - Start);
	}

	return Path;
}

UActorSpawnWidget::UActorSpawnWidget()
	: UWidget("Actor Spawn Widget")
	, UIManager(&UUIManager::GetInstance())
{
	srand(static_cast<unsigned int>(time(nullptr)));
}

UActorSpawnWidget::~UActorSpawnWidget() = default;

void UActorSpawnWidget::Initialize()
{
	UIManager = &UUIManager::GetInstance();
}

void UActorSpawnWidget::Update()
{
	// 필요시 업데이트 로직 추가
}

UWorld* UActorSpawnWidget::GetCurrentWorld() const
{
	if (!UIManager)
		return nullptr;

	return UIManager->GetWorld();
}

const char* UActorSpawnWidget::GetActorTypeName(int32 TypeIndex) const
{
	switch (static_cast<EActorType>(TypeIndex))
	{
	case EActorType::Empty: return "Actor (Empty)";
	case EActorType::StaticMesh: return "StaticMeshActor";
	case EActorType::Decal: return "DecalActor";
	default: return "Unknown";
	}
}

FVector UActorSpawnWidget::GenerateRandomLocation() const
{
	float RandomX = SpawnRangeMin + (static_cast<float>(rand()) / RAND_MAX) * (SpawnRangeMax - SpawnRangeMin);
	float RandomY = SpawnRangeMin + (static_cast<float>(rand()) / RAND_MAX) * (SpawnRangeMax - SpawnRangeMin);
	float RandomZ = SpawnRangeMin + (static_cast<float>(rand()) / RAND_MAX) * (SpawnRangeMax - SpawnRangeMin);

	return FVector(RandomX, RandomY, RandomZ);
}

float UActorSpawnWidget::GenerateRandomScale() const
{
	if (!bRandomScale)
		return 1.0f;

	return MinScale + (static_cast<float>(rand()) / RAND_MAX) * (MaxScale - MinScale);
}

FQuat UActorSpawnWidget::GenerateRandomRotation() const
{
	if (!bRandomRotation)
		return FQuat::Identity();

	float RandomPitch = (static_cast<float>(rand()) / RAND_MAX) * 360.0f - 180.0f;
	float RandomYaw = (static_cast<float>(rand()) / RAND_MAX) * 360.0f - 180.0f;
	float RandomRoll = (static_cast<float>(rand()) / RAND_MAX) * 360.0f - 180.0f;

	return FQuat::MakeFromEuler(FVector(RandomPitch, RandomYaw, RandomRoll));
}

void UActorSpawnWidget::RenderWidget()
{
	ImGui::Text("Actor Spawner");
	ImGui::Spacing();

	// Actor Type Selection (RadioButtons)
	ImGui::Text("Actor Type:");
	ImGui::RadioButton("Actor (Empty)", &SelectedActorType, static_cast<int32>(EActorType::Empty));
	ImGui::RadioButton("StaticMeshActor", &SelectedActorType, static_cast<int32>(EActorType::StaticMesh));
	ImGui::RadioButton("DecalActor", &SelectedActorType, static_cast<int32>(EActorType::Decal));

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// StaticMeshActor 전용 설정
	if (SelectedActorType == static_cast<int32>(EActorType::StaticMesh))
	{
		auto& ResourceManager = UResourceManager::GetInstance();
		CachedMeshFilePaths = ResourceManager.GetAllStaticMeshFilePaths();

		TArray<FString> DisplayNames;
		DisplayNames.reserve(CachedMeshFilePaths.size());
		for (const FString& Path : CachedMeshFilePaths)
		{
			DisplayNames.push_back(GetBaseNameNoExt(Path));
		}

		TArray<const char*> Items;
		Items.reserve(DisplayNames.size());
		for (const FString& Name : DisplayNames)
		{
			Items.push_back(Name.c_str());
		}

		// 기본 선택: Cube
		if (SelectedMeshIndex == -1 && !CachedMeshFilePaths.empty())
		{
			for (int32 i = 0; i < static_cast<int32>(CachedMeshFilePaths.size()); ++i)
			{
				if (GetBaseNameNoExt(CachedMeshFilePaths[i]) == "Cube")
				{
					SelectedMeshIndex = i;
					break;
				}
			}
		}

		ImGui::Text("Static Mesh:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(220);
		ImGui::Combo("##StaticMeshList", &SelectedMeshIndex, Items.data(), static_cast<int>(Items.size()));
		ImGui::SameLine();
		if (ImGui::Button("Clear##StaticMesh"))
		{
			SelectedMeshIndex = -1;
		}
	}

	// DecalActor 전용 설정
	if (SelectedActorType == static_cast<int32>(EActorType::Decal))
	{
		auto& ResourceManager = UResourceManager::GetInstance();
		CachedDecalTexturePaths = ResourceManager.GetAllTextureFilePaths();

		TArray<FString> DisplayNames;
		DisplayNames.reserve(CachedDecalTexturePaths.size());
		for (const FString& Path : CachedDecalTexturePaths)
		{
			DisplayNames.push_back(GetBaseNameNoExt(Path));
		}

		TArray<const char*> Items;
		Items.reserve(DisplayNames.size());
		for (const FString& Name : DisplayNames)
		{
			Items.push_back(Name.c_str());
		}

		// 기본 선택: DefaultDecalTexture
		if (SelectedDecalTextureIndex == -1 && !CachedDecalTexturePaths.empty())
		{
			for (int32 i = 0; i < static_cast<int32>(CachedDecalTexturePaths.size()); ++i)
			{
				if (GetBaseNameNoExt(CachedDecalTexturePaths[i]) == "DefaultDecalTexture")
				{
					SelectedDecalTextureIndex = i;
					break;
				}
			}
			// 못 찾으면 첫 번째 텍스처 선택
			if (SelectedDecalTextureIndex == -1)
			{
				SelectedDecalTextureIndex = 0;
			}
		}

		ImGui::Text("Decal Texture:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(220);
		ImGui::Combo("##DecalTextureList", &SelectedDecalTextureIndex, Items.data(), static_cast<int>(Items.size()));
		ImGui::SameLine();
		if (ImGui::Button("Clear##DecalTexture"))
		{
			SelectedDecalTextureIndex = -1;
		}

		// Size 설정
		ImGui::Text("Size:");
		ImGui::SetNextItemWidth(80);
		ImGui::DragFloat("X (Depth)##DecalSize", &DecalSize.X, 0.1f, 0.1f, 100.0f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::DragFloat("Y (Width)##DecalSize", &DecalSize.Y, 0.1f, 0.1f, 100.0f);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		ImGui::DragFloat("Z (Height)##DecalSize", &DecalSize.Z, 0.1f, 0.1f, 100.0f);

		// Opacity 설정
		ImGui::Text("Opacity:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(200);
		ImGui::SliderFloat("##DecalOpacity", &DecalOpacity, 0.0f, 1.0f);

		// Auto Fade In 설정
		ImGui::Checkbox("Auto Fade In", &bAutoFadeIn);
		if (bAutoFadeIn)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100);
			ImGui::DragFloat("Duration (s)##FadeIn", &FadeInDuration, 0.1f, 0.1f, 10.0f);
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// ========================================
	// 공통 Spawn 설정
	// ========================================
	ImGui::Text("Spawn Settings");

	// 스폰 개수
	ImGui::Text("Number of Spawn:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	ImGui::InputInt("##NumberOfSpawn", &NumberOfSpawn);
	NumberOfSpawn = max(1, min(100, NumberOfSpawn));

	// 스폰 위치 범위
	ImGui::Text("Position Range:");
	ImGui::SetNextItemWidth(100);
	ImGui::DragFloat("Min##SpawnRange", &SpawnRangeMin, 0.1f, -50.0f, SpawnRangeMax - 0.1f);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::DragFloat("Max##SpawnRange", &SpawnRangeMax, 0.1f, SpawnRangeMin + 0.1f, 50.0f);

	// 랜덤 회전
	ImGui::Checkbox("Random Rotation", &bRandomRotation);

	// 랜덤 스케일 (DecalActor는 스케일 사용 안 함)
	if (SelectedActorType != static_cast<int32>(EActorType::Decal))
	{
		ImGui::Checkbox("Random Scale", &bRandomScale);
		if (bRandomScale)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80);
			ImGui::DragFloat("Min##Scale", &MinScale, 0.01f, 0.1f, MaxScale - 0.01f);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80);
			ImGui::DragFloat("Max##Scale", &MaxScale, 0.01f, MinScale + 0.01f, 10.0f);
		}
	}

	ImGui::Spacing();

	// ========================================
	// Spawn 버튼
	// ========================================
	if (ImGui::Button("Spawn Actors", ImVec2(200, 30)))
	{
		SpawnActors();
	}

	ImGui::Spacing();
	ImGui::Separator();

	// ========================================
	// 월드 상태 정보
	// ========================================
	UWorld* World = GetCurrentWorld();
	if (World)
	{
		ImGui::Text("World Status: Connected");
		ImGui::Text("Current Actors: %zu", World->GetActors().size());
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "World Status: Not Available");
	}
}

void UActorSpawnWidget::SpawnActors() const
{
	UWorld* World = GetCurrentWorld();
	if (!World)
	{
		UE_LOG("ActorSpawn: No World available for spawning");
		return;
	}

	UE_LOG("ActorSpawn: Spawning %d %s actors", NumberOfSpawn, GetActorTypeName(SelectedActorType));

	for (int32 i = 0; i < NumberOfSpawn; i++)
	{
		switch (static_cast<EActorType>(SelectedActorType))
		{
		case EActorType::Empty:
			SpawnEmptyActor(World);
			break;
		case EActorType::StaticMesh:
			SpawnStaticMeshActor(World);
			break;
		case EActorType::Decal:
			SpawnDecalActor(World);
			break;
		}
	}
}

void UActorSpawnWidget::SpawnEmptyActor(UWorld* World) const
{
	FVector SpawnLocation = GenerateRandomLocation();
	FQuat SpawnRotation = GenerateRandomRotation();
	float SpawnScale = GenerateRandomScale();
	FVector SpawnScaleVec(SpawnScale, SpawnScale, SpawnScale);

	FTransform SpawnTransform(SpawnLocation, SpawnRotation, SpawnScaleVec);

	AActor* NewActor = World->SpawnActor<AActor>(SpawnTransform);
	if (NewActor)
	{
		FString ActorName = World->GenerateUniqueActorName("Actor");
		NewActor->SetName(ActorName);
		UE_LOG("ActorSpawn: Created Empty Actor '%s' at (%.2f, %.2f, %.2f)",
			ActorName.c_str(), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
	}
}

void UActorSpawnWidget::SpawnStaticMeshActor(UWorld* World) const
{
	FVector SpawnLocation = GenerateRandomLocation();
	FQuat SpawnRotation = GenerateRandomRotation();
	float SpawnScale = GenerateRandomScale();
	FVector SpawnScaleVec(SpawnScale, SpawnScale, SpawnScale);

	FTransform SpawnTransform(SpawnLocation, SpawnRotation, SpawnScaleVec);

	AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(SpawnTransform);
	if (NewActor)
	{
		// 메시 설정
		FString MeshPath = "Data/Cube.obj";
		if (SelectedMeshIndex >= 0 && SelectedMeshIndex < static_cast<int32>(CachedMeshFilePaths.size()))
		{
			MeshPath = CachedMeshFilePaths[SelectedMeshIndex];
		}

		if (auto* StaticMeshComp = NewActor->GetStaticMeshComponent())
		{
			StaticMeshComp->SetStaticMesh(MeshPath);

			// 충돌 컴포넌트 설정
			if (GetBaseNameNoExt(MeshPath) == "Sphere")
			{
				NewActor->SetCollisionComponent(EPrimitiveType::Sphere);
			}
			else
			{
				NewActor->SetCollisionComponent();
			}
		}

		FString ActorName = World->GenerateUniqueActorName(GetBaseNameNoExt(MeshPath));
		NewActor->SetName(ActorName);

		UE_LOG("ActorSpawn: Created StaticMeshActor '%s' at (%.2f, %.2f, %.2f) with mesh '%s'",
			ActorName.c_str(), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z, MeshPath.c_str());
	}
}

void UActorSpawnWidget::SpawnDecalActor(UWorld* World) const
{
	// DecalActor는 스케일을 DecalSize로 직접 제어
	FVector SpawnLocation = GenerateRandomLocation();
	FQuat SpawnRotation = GenerateRandomRotation();

	FTransform SpawnTransform(SpawnLocation, SpawnRotation, FVector::One());

	ADecalActor* NewActor = World->SpawnActor<ADecalActor>(SpawnTransform);
	if (NewActor)
	{
		UDecalComponent* DecalComp = NewActor->GetDecalComponent();
		if (DecalComp)
		{
			// 텍스처 설정
			if (SelectedDecalTextureIndex >= 0 && SelectedDecalTextureIndex < static_cast<int32>(CachedDecalTexturePaths.size()))
			{
				FString TexturePath = CachedDecalTexturePaths[SelectedDecalTextureIndex];
				DecalComp->SetDecalTexture(TexturePath);

				UE_LOG("ActorSpawn: Decal texture set to '%s'", TexturePath.c_str());
			}

			// Size 설정
			DecalComp->SetDecalSize(DecalSize);

			// Opacity 설정
			DecalComp->SetBaseOpacity(DecalOpacity);

			// Auto Fade In 설정
			if (bAutoFadeIn)
			{
				DecalComp->StartFadeIn(FadeInDuration);
			}
		}

		FString ActorName = World->GenerateUniqueActorName("DecalActor");
		NewActor->SetName(ActorName);

		UE_LOG("ActorSpawn: Created DecalActor '%s' at (%.2f, %.2f, %.2f) Size(%.2f, %.2f, %.2f) Opacity=%.2f",
			ActorName.c_str(), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z,
			DecalSize.X, DecalSize.Y, DecalSize.Z, DecalOpacity);
	}
}
