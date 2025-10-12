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

    // Scene 로드 시 생성자가 만든 기본 컴포넌트를 삭제하기 위한 메서드
    void ClearDefaultComponents();

    UObject* Duplicate() override;
    UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
    void DuplicateSubObjects() override;

protected:
    // [PIE] 부모 Duplicate 호출하고 Root를 StaticMeshComponent 에 넣어주면 될듯
    UStaticMeshComponent* StaticMeshComponent;
};

