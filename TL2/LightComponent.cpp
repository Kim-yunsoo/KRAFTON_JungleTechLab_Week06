#include "pch.h"
#include "LightComponent.h"

ULightComponent::ULightComponent() : LightColor(1,0,0), AttenuationRadius(1.7f), Intensity(1.0f)
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
