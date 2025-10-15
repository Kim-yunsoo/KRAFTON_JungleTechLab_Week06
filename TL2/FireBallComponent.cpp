#include "pch.h"
#include "FireBallComponent.h"

UFireBallComponent::UFireBallComponent()
{
}

UFireBallComponent::~UFireBallComponent()
{
}

UObject* UFireBallComponent::Duplicate(FObjectDuplicationParameters Parameters)
{
	auto DupObject = static_cast<UFireBallComponent*>(Super_t::Duplicate(Parameters));

	//TODO

	return DupObject;
}