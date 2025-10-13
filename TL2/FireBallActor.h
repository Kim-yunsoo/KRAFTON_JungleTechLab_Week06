#pragma once
#include "StaticMeshActor.h"

class AFireBallActor : public AStaticMeshActor
{	
public:
	DECLARE_CLASS(AFireBallActor, AStaticMeshActor)

	AFireBallActor();

protected:
	~AFireBallActor();

	UObject* Duplicate(FObjectDuplicationParameters Parameters);
};

