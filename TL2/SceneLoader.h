#pragma once

#include <fstream>
#include <sstream>

#include "nlohmann/json.hpp"   
#include "Vector.h"
#include "UEContainer.h"
using namespace json;

// ========================================
// Version 1 (Legacy - 하위 호환용)
// ========================================
struct FPrimitiveData
{
    uint32 UUID = 0;
    FVector Location;
    FVector Rotation;
    FVector Scale;
    FString Type;
    FString ObjStaticMeshAsset;
};

// ========================================
// Version 2 (Component Hierarchy Support)
// ========================================
struct FComponentData
{
    uint32 UUID = 0;
    uint32 OwnerActorUUID = 0;
    uint32 ParentComponentUUID = 0;  // 0이면 RootComponent (부모 없음)
    FString Type;  // "StaticMeshComponent", "AABoundingBoxComponent" 등

    // Transform
    FVector RelativeLocation;
    FVector RelativeRotation;
    FVector RelativeScale;

    // Type별 속성 (StaticMeshComponent 전용)
    FString StaticMesh;  // Asset path
    TArray<FString> Materials;

    // DecalComponent 전용 속성
    FString DecalTexture;  // Decal texture path
    FVector DecalSize;  // Decal box size
    int32 SortOrder = 0;
    float FadeInDuration = 1.0f;
    float FadeStartDelay = 1.0f;
    float FadeDuration = 1.0f;
};

struct FActorData
{
    uint32 UUID = 0;
    FString Type;  // "StaticMeshActor" 등
    FString Name;
    uint32 RootComponentUUID = 0;
};

struct FPerspectiveCameraData
{
    FVector Location;
	FVector Rotation;
	float FOV;
	float NearClip;
	float FarClip;
};

struct FSceneData
{
    uint32 Version = 2;
    uint32 NextUUID = 0;
    TArray<FActorData> Actors;
    TArray<FComponentData> Components;
    FPerspectiveCameraData Camera;
};

class FSceneLoader
{
public:
    // Version 2 API
    static FSceneData LoadV2(const FString& FileName);
    static void SaveV2(const FSceneData& SceneData, const FString& SceneName);

    // Legacy Version 1 API (하위 호환)
    static TArray<FPrimitiveData> Load(const FString& FileName, FPerspectiveCameraData* OutCameraData);
    static void Save(TArray<FPrimitiveData> InPrimitiveData, const FPerspectiveCameraData* InCameraData, const FString& SceneName);

    static bool TryReadNextUUID(const FString& FilePath, uint32& OutNextUUID);

private:
    static TArray<FPrimitiveData> Parse(const JSON& Json);
    static FSceneData ParseV2(const JSON& Json);
};