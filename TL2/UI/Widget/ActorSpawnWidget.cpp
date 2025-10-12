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
#include <filesystem>

using std::max;
using std::min;
namespace fs = std::filesystem;

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

TArray<FString> UActorSpawnWidget::GetDecalFiles() const
{
	TArray<FString> decalFiles;

	fs::path DecalPath = "Editor/Decal/";
	if (fs::exists(DecalPath) && fs::is_directory(DecalPath))
	{
		for (const auto& File : fs::directory_iterator(DecalPath))
		{
			if (File.is_regular_file())
			{
				auto Filename = File.path().filename().string();

				if (Filename.ends_with(".dds") || Filename.ends_with(".png") || Filename.ends_with(".jpg"))
				{
					FString RelativePath = DecalPath.string() + Filename;
					decalFiles.push_back(RelativePath);
				}
			}
		}
	}
	return decalFiles;
}

void UActorSpawnWidget::RenderWidget()
{
	ImGui::Text("Actor Spawner");
	ImGui::Spacing();

	// Actor Type Selection (ComboBox)
	ImGui::Text("Actor Type:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(220);
	const char* ActorTypeItems[] = { "Actor (Empty)", "StaticMeshActor", "DecalActor", "PerspectiveDecalActor"};
	ImGui::Combo("##ActorType", &SelectedActorType, ActorTypeItems, IM_ARRAYSIZE(ActorTypeItems));

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
	if (SelectedActorType == static_cast<int32>(EActorType::Decal) || SelectedActorType == static_cast<int32>(EActorType::PerspectiveDecal))
	{
		// GetDecalFiles()를 사용하여 Decal 텍스처 경로를 가져옵니다
		CachedDecalTexturePaths = GetDecalFiles();

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

		// Fade 설정 (UTargetActorTransformWidget의 Decal Component Settings와 동일)
		ImGui::Separator();
		ImGui::Text("Fade Settings");

		ImGui::DragFloat("Max Alpha", &MaxAlpha, 0.05f, 0.0f, 1.0f, "%.2f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Decal의 Max Alpha 값을 조절합니다");
		}

		ImGui::DragFloat("Fade In Duration", &FadeInDuration, 0.05f, 0.0f, 100.0f, "%.2f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Fade In 지속 시간을 초 단위로 설정합니다.");
		}

		ImGui::DragFloat("Fade Start Delay", &FadeStartDelay, 0.05f, 0.0f, 100.0f, "%.2f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Decal 지속 시간을 초 단위로 설정합니다.");
		}

		ImGui::DragFloat("Fade Out Duration", &FadeOutDuration, 0.05f, 0.0f, 100.0f, "%.2f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Fade Out 지속 시간을 초 단위로 설정합니다.");
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
			SpawnDecalActor(World, true);
			break;
		case EActorType::PerspectiveDecal:
			SpawnDecalActor(World, false);
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

void UActorSpawnWidget::SpawnDecalActor(UWorld* World, bool bIsOrtho) const
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

			// Fade 설정
			DecalComp->SetFadeInDuration(FadeInDuration);
			DecalComp->SetFadeStartDelay(FadeStartDelay);
			DecalComp->SetFadeDuration(FadeOutDuration);
			DecalComp->SetMaxAlpha(MaxAlpha); 
			DecalComp->SetProjectionMatrixFlag(bIsOrtho); 
		}

		FString ActorName = World->GenerateUniqueActorName("DecalActor");
		NewActor->SetName(ActorName);

		UE_LOG("ActorSpawn: Created DecalActor '%s' at (%.2f, %.2f, %.2f)  FadeIn=%.2f Delay=%.2f FadeOut=%.2f",
			ActorName.c_str(), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z, FadeInDuration, FadeStartDelay, FadeOutDuration);
	}
}
