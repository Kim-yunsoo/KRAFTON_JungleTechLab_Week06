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

protected:
    ~AFireBallActor() override;

protected:
	UPointLightComponent* PointLightComponent;

};

