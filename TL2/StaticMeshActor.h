#pragma once
#include "Actor.h"
#include "StaticMeshComponent.h"
#include "Enums.h"
class AStaticMeshActor : public AActor
{
public:
    DECLARE_CLASS(AStaticMeshActor, AActor)

    AStaticMeshActor();
    virtual void Tick(float DeltaTime) override;
protected:
    ~AStaticMeshActor() override;

public:
    virtual bool DeleteComponent(USceneComponent* ComponentToDelete) override;

    UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
    void SetStaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent);
	void SetCollisionComponent(EPrimitiveType InType = EPrimitiveType::Default);

    UObject* Duplicate() override;
    UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
    void DuplicateSubObjects() override;

protected:
    // [PIE] 부모 Duplicate 호출하고 Root를 StaticMeshComponent 에 넣어주면 될듯
    UStaticMeshComponent* StaticMeshComponent;
};

