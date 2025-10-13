#pragma once

#include "PrimitiveComponent.h"
class UFireBallComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UFireBallComponent, UPrimitiveComponent)
	UFireBallComponent();

protected:
	~UFireBallComponent() override;

	UObject* Duplicate(FObjectDuplicationParameters Parameters) override; 
};

