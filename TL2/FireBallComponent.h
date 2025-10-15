#pragma once

#include "PrimitiveComponent.h"
class UFireBallComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UFireBallComponent, UPrimitiveComponent)
	UFireBallComponent();

	UObject* Duplicate(FObjectDuplicationParameters Parameters) override; 
protected:
	~UFireBallComponent() override;
	
};

