// DecalActor.cpp
#include "pch.h"
#include "DecalActor.h"
#include "OBoundingBoxComponent.h"
#include "AABoundingBoxComponent.h"
#include "DecalComponent.h"
#include "ObjectFactory.h"
#include "BillboardComponent.h"


ADecalActor::ADecalActor()
{
    // DecalComponent 생성 및 컴포넌트 생성
    DecalComponent = CreateDefaultSubobject<UDecalComponent>(FName("DecalComponent")); 
    DecalComponent->SetupAttachment(RootComponent);

    CollisionComponent = CreateDefaultSubobject<UAABoundingBoxComponent>(FName("CollisionBox"));
    CollisionComponent->SetupAttachment(RootComponent);

    DecalVolumeComponent = CreateDefaultSubobject< UOBoundingBoxComponent>(FName("DecalVolumeComponent"));
    DecalVolumeComponent->SetupAttachment(RootComponent);

    BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(FName("BillboardComponent"));
    BillboardComponent->SetWorldLocation(FVector(0.f, 0.f, 0.f));
    BillboardComponent->SetupAttachment(RootComponent);
}

ADecalActor::~ADecalActor()
{
}

void ADecalActor::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
    if (CollisionComponent && DecalComponent && DecalComponent->GetDecalBoxMesh() && DecalComponent->GetDecalBoxMesh()->GetStaticMeshAsset())
    {
        CollisionComponent->SetFromVertices(DecalComponent->GetDecalBoxMesh()->GetStaticMeshAsset()->Vertices);
    }

    // OBB update
    if (DecalVolumeComponent)
    {
        DecalVolumeComponent->UpdateFromWorld(GetWorldMatrix());
    }
     
    // TODO Duplcate할 때 DecalVolumeComponent가 사라짐
    if (!DecalVolumeComponent)
    {
        DecalVolumeComponent = CreateDefaultSubobject<UOBoundingBoxComponent>(FName("DecalVolumeComponent"));
        if (DecalVolumeComponent)
        {
            DecalVolumeComponent->SetupAttachment(RootComponent);
        }
    } 

    //StatTick(DeltaTime);
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
    ADecalActor* NewActor = NewObject<ADecalActor>(*this);

    if (NewActor)
    {
        NewActor->OverlappingActors.Empty();
        // DecalComponent는 부모가 Duplicate에서 처리됨
       // NewActor->DecalComponent = static_cast<UDecalComponent*>(NewActor->GetRootComponent());

        // 생성자가 만든 컴포넌트 삭제 
        if (NewActor->DecalComponent)
        {
            NewActor->OwnedComponents.Remove(NewActor->DecalComponent);
            ObjectFactory::DeleteObject(NewActor->DecalComponent);
            NewActor->DecalComponent = nullptr;
        }
        if (NewActor->DecalVolumeComponent)
        {
            NewActor->OwnedComponents.Remove(NewActor->DecalVolumeComponent);
            ObjectFactory::DeleteObject(NewActor->DecalVolumeComponent);
            NewActor->DecalVolumeComponent = nullptr;
        }
        if (NewActor->CollisionComponent)
        {
            NewActor->OwnedComponents.Remove(NewActor->CollisionComponent);
            ObjectFactory::DeleteObject(NewActor->CollisionComponent);
            NewActor->CollisionComponent = nullptr;
        }
        if (NewActor->RootComponent)
        {
            NewActor->OwnedComponents.Remove(NewActor->RootComponent);
            ObjectFactory::DeleteObject(NewActor->RootComponent);
            NewActor->RootComponent = nullptr;
        }
        NewActor->OwnedComponents.clear();

        if (this->GetRootComponent())
        {
            NewActor->RootComponent = Cast<USceneComponent>(this->GetRootComponent()->Duplicate());
        }

        // Rebuild owned components and rebind cached pointers
        NewActor->DuplicateSubObjects();
    }

    return NewActor; 
}


void ADecalActor::DuplicateSubObjects()
{
    Super_t::DuplicateSubObjects();

    // Rebind component cache after duplication
    DecalComponent = nullptr;
    DecalVolumeComponent = nullptr;
    CollisionComponent = nullptr;

    for (UActorComponent* Comp : OwnedComponents)
    {
        if (!Comp) continue;
        if (!DecalComponent)
        {
            if (auto* DC = Cast<UDecalComponent>(Comp))
            {
                DecalComponent = DC;
                continue;
            }
        }
        if (!DecalVolumeComponent)
        {
            if (auto* OBB = Cast<UOBoundingBoxComponent>(Comp))
            {
                DecalVolumeComponent = OBB;
                continue;
            }
        }
        if (!CollisionComponent)
        {
            if (auto* AABB = Cast<UAABoundingBoxComponent>(Comp))
            {
                CollisionComponent = AABB;
                continue;
            }
        }
    }
}
