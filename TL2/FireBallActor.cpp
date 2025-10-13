#include "pch.h"
#include "FireBallActor.h"

AFireBallActor::AFireBallActor()
{
}

AFireBallActor::~AFireBallActor()
{
}

UObject* AFireBallActor::Duplicate(FObjectDuplicationParameters Parameters)
{
	auto DupActor = static_cast<AFireBallActor*>(Super_t::Duplicate(Parameters));
	//TODO

	return DupActor;
}