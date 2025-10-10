// DecalActor.h
#pragma once
#include "Actor.h"
#include "DecalComponent.h"

class UOBoundingBoxComponent;

class ADecalActor : public AActor
{
public:
    DECLARE_CLASS(ADecalActor, AActor)

    ADecalActor();
    virtual ~ADecalActor() override;

    virtual void Tick(float DeltaTime) override;

    UDecalComponent* GetDecalComponent() const { return DecalComponent; }
    void SetDecalComponent(UDecalComponent* InDecalComponent);

    void CheckAndAddOverlappingActors(AActor* OverlappingActor); 

    virtual bool DeleteComponent(USceneComponent* ComponentToDelete) override;

    UObject* Duplicate() override;
    void DuplicateSubObjects() override;
    
    TArray<AActor*> GetOverlappingActors() const { return OverlappingActors; }

protected:
    UDecalComponent* DecalComponent;
    UOBoundingBoxComponent* DecalVolumeComponent;

    TArray<AActor*> OverlappingActors;
};
