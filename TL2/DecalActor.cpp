// DecalActor.cpp
#include "pch.h"
#include "DecalActor.h"
#include "OBoundingBoxComponent.h"

IMPLEMENT_CLASS(ADecalActor)

ADecalActor::ADecalActor()
{
    // DecalComponent 생성 및 컴포넌트 생성
    DecalComponent = CreateDefaultSubobject<UDecalComponent>(FName("DecalComponent")); 
    DecalComponent->SetupAttachment(RootComponent);


    CollisionComponent = CreateDefaultSubobject<UAABoundingBoxComponent>(FName("CollisionBox"));
    CollisionComponent->SetupAttachment(RootComponent);

    DecalVolumeComponent = CreateDefaultSubobject< UOBoundingBoxComponent>(FName("DecalVolume"));
    DecalVolumeComponent->SetupAttachment(RootComponent);

}

ADecalActor::~ADecalActor()
{
}

void ADecalActor::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
    if(CollisionComponent)
    CollisionComponent->SetFromVertices(DecalComponent->GetDecalBoxMesh()->GetStaticMeshAsset()->Vertices);

    // OBB update
    DecalVolumeComponent->UpdateFromWorld(GetWorldMatrix());
    
}

void ADecalActor::SetDecalComponent(UDecalComponent* InDecalComponent)
{
    DecalComponent = InDecalComponent;
    DecalComponent->SetupAttachment(RootComponent);
}

void ADecalActor::CheckAndAddOverlappingActors(AActor* OverlappingActor)
{
    if (!OverlappingActor)
    {
        return;
    }

    if (!OverlappingActor->CollisionComponent)
    {
        return;
    } 

    if ( OverlappingActor->IsA<ADecalActor>())
    {
        return;
    }

    FBound AABB = OverlappingActor->CollisionComponent->GetWorldBoundFromCube();
    //TODO: 지금은 AABB이고 OBB로 바꿔야 된다.
    //FBound OBB = CollisionComponent->GetWorldBoundFromCube();
     
     
    if (DecalVolumeComponent->IntersectWithAABB(AABB))
    { 
        OverlappingActors.AddUnique(OverlappingActor); 
    }  
}

bool ADecalActor::DeleteComponent(USceneComponent* ComponentToDelete)
{
    if (ComponentToDelete == DecalComponent)
    {
        // 디칼 컴포넌트는 삭제할 수 없음
        return false;
    }
    return AActor::DeleteComponent(ComponentToDelete);
}

UObject* ADecalActor::Duplicate()
{
    ADecalActor* NewActor = static_cast<ADecalActor*>(AActor::Duplicate());
    if (NewActor)
    {
        // DecalComponent는 부모가 Duplicate에서 처리됨
        NewActor->DecalComponent = static_cast<UDecalComponent*>(NewActor->GetRootComponent());
    }
    return NewActor;
}

void ADecalActor::DuplicateSubObjects()
{
    AActor::DuplicateSubObjects();
}
