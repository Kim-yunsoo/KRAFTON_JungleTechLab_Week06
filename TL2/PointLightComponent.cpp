#include "pch.h"
#include "PointLightComponent.h"

UPointLightComponent::UPointLightComponent() :LightFalloffExponent(0.7)
{
}

UObject* UPointLightComponent::Duplicate(FObjectDuplicationParameters Parameters)
{
	UPointLightComponent* DupObject = static_cast<UPointLightComponent*>(Super_t::Duplicate(Parameters));
	
	DupObject->LightFalloffExponent = LightFalloffExponent;
	
	return DupObject;
}

UPointLightComponent::~UPointLightComponent()
{
}
	