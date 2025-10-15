#pragma once
#include "StaticMeshActor.h"

class UPointLightComponent;

class AFireBallActor : public AStaticMeshActor
{	 
public:
    DECLARE_CLASS(AFireBallActor, AStaticMeshActor)

    AFireBallActor();

    UObject* Duplicate(FObjectDuplicationParameters Parameters) override;

    virtual void Tick(float DeltaTime) override;

    void SetPointLightComponent(UPointLightComponent* InComponent) { PointLightComponent = InComponent; }
    UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }

    // Scene 로드 시 생성자가 만든 기본 컴포넌트를 삭제하기 위한 메서드
    void ClearDefaultComponents();

protected:
    ~AFireBallActor() override;

protected:
	UPointLightComponent* PointLightComponent;

};

