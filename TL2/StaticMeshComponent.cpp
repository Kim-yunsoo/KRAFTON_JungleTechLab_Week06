#include "pch.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "Shader.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "ObjManager.h"
#include "SceneLoader.h"

UStaticMeshComponent::UStaticMeshComponent()
{
    SetMaterial("StaticMeshShader.hlsl");
}

UStaticMeshComponent::~UStaticMeshComponent()
{

}

void UStaticMeshComponent::Render(URenderer* Renderer, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
    if (StaticMesh)
    {
        Renderer->UpdateConstantBuffer(GetWorldMatrix(), ViewMatrix, ProjectionMatrix);
        Renderer->PrepareShader(GetMaterial()->GetShader());
        Renderer->DrawIndexedPrimitiveComponent(GetStaticMesh(), D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST, MaterailSlots);
    }
}

void UStaticMeshComponent::SetStaticMesh(const FString& PathFileName)
{
	StaticMesh = FObjManager::LoadObjStaticMesh(PathFileName);
    
    const TArray<FGroupInfo>& GroupInfos = StaticMesh->GetMeshGroupInfo();
    if (MaterailSlots.size() < GroupInfos.size())
    {
        MaterailSlots.resize(GroupInfos.size());
    }

    // MaterailSlots.size()가 GroupInfos.size() 보다 클 수 있기 때문에, GroupInfos.size()로 설정
    for (int i = 0; i < GroupInfos.size(); ++i) 
    {
        if (MaterailSlots[i].bChangedByUser == false)
        {
            MaterailSlots[i].MaterialName = GroupInfos[i].InitialMaterialName;
        }
    }
}

FBound UStaticMeshComponent::GetWorldBoundingBox() const
{
    if (!StaticMesh)
    {
        // StaticMesh가 없으면 기본 작은 박스 반환
        FVector WorldPos = GetWorldLocation();
        return FBound(WorldPos - FVector(0.1f, 0.1f, 0.1f),
            WorldPos + FVector(0.1f, 0.1f, 0.1f));
    }

    // StaticMesh가 제공하는 로컬 바운드 사용
    FBound LocalBound = StaticMesh->GetLocalBound();

    // 로컬 AABB의 8개 꼭짓점을 월드로 변환
    FVector LocalMin = LocalBound.Min;
    FVector LocalMax = LocalBound.Max;

    FMatrix WorldMatrix = GetWorldMatrix();
    FVector Corners[8] = {
        FVector(LocalMin.X, LocalMin.Y, LocalMin.Z),
        FVector(LocalMax.X, LocalMin.Y, LocalMin.Z),
        FVector(LocalMin.X, LocalMax.Y, LocalMin.Z),
        FVector(LocalMax.X, LocalMax.Y, LocalMin.Z),
        FVector(LocalMin.X, LocalMin.Y, LocalMax.Z),
        FVector(LocalMax.X, LocalMin.Y, LocalMax.Z),
        FVector(LocalMin.X, LocalMax.Y, LocalMax.Z),
        FVector(LocalMax.X, LocalMax.Y, LocalMax.Z),
    };

    // 월드 공간 AABB 계산
    FVector WorldMin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector WorldMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int i = 0; i < 8; ++i)
    {
        // 로컬 → 월드 변환
        FVector4 LocalPos4(Corners[i].X, Corners[i].Y, Corners[i].Z, 1.0f);
        FVector4 WorldPos4 = WorldMatrix.TransformPosition(LocalPos4);
        FVector WorldCorner(WorldPos4.X, WorldPos4.Y, WorldPos4.Z);

        WorldMin.X = FMath::Min(WorldMin.X, WorldCorner.X);
        WorldMin.Y = FMath::Min(WorldMin.Y, WorldCorner.Y);
        WorldMin.Z = FMath::Min(WorldMin.Z, WorldCorner.Z);

        WorldMax.X = FMath::Max(WorldMax.X, WorldCorner.X);
        WorldMax.Y = FMath::Max(WorldMax.Y, WorldCorner.Y);
        WorldMax.Z = FMath::Max(WorldMax.Z, WorldCorner.Z);
    }

    return FBound(WorldMin, WorldMax);
}

FOrientedBox UStaticMeshComponent::GetWorldOrientedBox() const
{
    if (!StaticMesh)
    {
        return FOrientedBox(GetWorldLocation(), FVector(0.1f, 0.1f, 0.1f), FQuat::Identity());
    }

    // StaticMesh가 제공하는 로컬 바운드 사용
    FBound LocalBound = StaticMesh->GetLocalBound();

    // OBB 구성
    FVector LocalMin = LocalBound.Min;
    FVector LocalMax = LocalBound.Max;
    FVector LocalCenter = (LocalMin + LocalMax) * 0.5f;
    FVector LocalHalfExtents = (LocalMax - LocalMin) * 0.5f;

    // 월드 변환 적용
    FMatrix WorldMatrix = GetWorldMatrix();
    FVector4 LocalCenter4(LocalCenter.X, LocalCenter.Y, LocalCenter.Z, 1.0f);
    FVector4 WorldCenter4 = WorldMatrix.TransformPosition(LocalCenter4);
    FVector WorldCenter(WorldCenter4.X, WorldCenter4.Y, WorldCenter4.Z);

    FVector WorldHalfExtents = LocalHalfExtents * GetWorldScale(); // 스케일 적용
    FQuat WorldRotation = GetWorldRotation();

    return FOrientedBox(WorldCenter, WorldHalfExtents, WorldRotation);
}

void UStaticMeshComponent::Serialize(bool bIsLoading, FPrimitiveData& InOut)
{
    // 0) 트랜스폼 직렬화/역직렬화는 상위(UPrimitiveComponent)에서 처리
    UPrimitiveComponent::Serialize(bIsLoading, InOut);

    if (bIsLoading)
    {
        // 1) 신규 포맷: ObjStaticMeshAsset가 있으면 우선 사용
        if (!InOut.ObjStaticMeshAsset.empty())
        {
            SetStaticMesh(InOut.ObjStaticMeshAsset);
            return;
        }

        // 2) 레거시 호환: Type을 "Data/<Type>.obj"로 매핑
        if (!InOut.Type.empty())
        {
            const FString LegacyPath = "Data/" + InOut.Type + ".obj";
            SetStaticMesh(LegacyPath);
        }
    }
    else
    {
        // 저장 시: 현재 StaticMesh가 있다면 실제 에셋 경로를 기록
        if (UStaticMesh* Mesh = GetStaticMesh())
        {
            InOut.ObjStaticMeshAsset = Mesh->GetAssetPathFileName();
        }
        else
        {
            InOut.ObjStaticMeshAsset.clear();
        }
        // Type은 상위(월드/액터) 정책에 따라 별도 기록 (예: "StaticMeshComp")
    }
}

void UStaticMeshComponent::SetMaterialByUser(const uint32 InMaterialSlotIndex, const FString& InMaterialName)
{
    assert((0 <= InMaterialSlotIndex && InMaterialSlotIndex < MaterailSlots.size()) && "out of range InMaterialSlotIndex");

    if (0 <= InMaterialSlotIndex && InMaterialSlotIndex < MaterailSlots.size())
    {
        MaterailSlots[InMaterialSlotIndex].MaterialName = InMaterialName;
        MaterailSlots[InMaterialSlotIndex].bChangedByUser = true;
    }
    else
    {
        UE_LOG("out of range InMaterialSlotIndex: %d", InMaterialSlotIndex);
    }

    assert(MaterailSlots[InMaterialSlotIndex].bChangedByUser == true);
}

UObject* UStaticMeshComponent::Duplicate()
{
    UStaticMeshComponent* DuplicatedComponent = Cast<UStaticMeshComponent>(NewObject(GetClass()));

    // 공통 속성 복사 (Transform, AttachChildren) - 부모 헬퍼 사용
    CopyCommonProperties(DuplicatedComponent);

    // StaticMeshComponent 전용 속성 복사
    DuplicatedComponent->Material = this->Material;
    DuplicatedComponent->StaticMesh = this->StaticMesh;
    DuplicatedComponent->MaterailSlots = this->MaterailSlots;

    DuplicatedComponent->DuplicateSubObjects();
    return DuplicatedComponent;
}

UObject* UStaticMeshComponent::Duplicate(FObjectDuplicationParameters Parameter)
{
    auto DupObject = static_cast<UStaticMeshComponent*>(Super_t::Duplicate(Parameter));
    
    DupObject->StaticMesh = StaticMesh;
    DupObject->MaterailSlots = MaterailSlots;
     
    return DupObject;
}

void UStaticMeshComponent::DuplicateSubObjects()
{
    // 부모의 깊은 복사 수행 (AttachChildren 재귀 복제)
    Super_t::DuplicateSubObjects();
}
