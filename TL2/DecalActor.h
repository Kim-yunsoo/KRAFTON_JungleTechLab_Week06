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

    // Scene 로드 시 생성자가 만든 기본 컴포넌트를 삭제하기 위한 메서드
    void ClearDefaultComponents();

    virtual bool DeleteComponent(USceneComponent* ComponentToDelete) override;

    UObject* Duplicate() override;
    void DuplicateSubObjects() override;

protected:
    UDecalComponent* DecalComponent;
};
