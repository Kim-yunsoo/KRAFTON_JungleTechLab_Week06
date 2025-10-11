// DecalActor.h
#pragma once
#include "Actor.h"
#include "DecalComponent.h"

class UOBoundingBoxComponent;
class UBillboardComponent;

class ADecalActor : public AActor
{
public:
    DECLARE_CLASS(ADecalActor, AActor)

    ADecalActor();
    virtual ~ADecalActor() override;

    virtual void Tick(float DeltaTime) override;

    UDecalComponent* GetDecalComponent() const { return DecalComponent; }
    void SetDecalComponent(UDecalComponent* InDecalComponent);

    virtual bool DeleteComponent(USceneComponent* ComponentToDelete) override;

    UObject* Duplicate() override;
    void DuplicateSubObjects() override;

protected:
    UDecalComponent* DecalComponent;
    UBillboardComponent* BillboardComponent;
};
