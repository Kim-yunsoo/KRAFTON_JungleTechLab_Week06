#include "pch.h"
#include "LightComponent.h"

ULightComponent::ULightComponent()
{
}

UObject* ULightComponent::Duplication(FObjectDuplicationParameters Parameters)
{
	ULightComponent* DupComp = static_cast<ULightComponent*>(Super_t::Duplicate(Parameters));

	DupComp->LightColor = LightColor;
	DupComp->AttenuationRadius = AttenuationRadius;
	DupComp->Intensity = Intensity;

	return DupComp;
}

ULightComponent::~ULightComponent()
{

}
