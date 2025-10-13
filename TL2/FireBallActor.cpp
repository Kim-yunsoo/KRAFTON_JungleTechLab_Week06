#include "pch.h"
#include "FireBallActor.h"
#include "PointLightComponent.h"

AFireBallActor::AFireBallActor()
{
	// Add Point Light Component
	PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("PointLight");
	PointLightComponent->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);  
}

AFireBallActor::~AFireBallActor()
{
	if (PointLightComponent)
	{
		ObjectFactory::DeleteObject(PointLightComponent);
	}
	PointLightComponent = nullptr;
}

UObject* AFireBallActor::Duplicate(FObjectDuplicationParameters Parameters)
{
	auto DupActor = static_cast<AFireBallActor*>(Super_t::Duplicate(Parameters));

	//TODO
	for (UActorComponent* Component : DupActor->OwnedComponents)
	{

		if (auto It = Parameters.DuplicationSeed.find(Component); It != Parameters.DuplicationSeed.end())
		{
			DupActor->OwnedComponents.emplace(static_cast<UActorComponent*>(It->second));
		}
		else
		{
			auto Params = InitStaticDuplicateObjectParams(Component, DupActor, FName::GetNone(), Parameters.DuplicationSeed, Parameters.CreatedObjects);
			auto DupComponent = static_cast<UActorComponent*>(Component->Duplicate(Params));

			DupActor->OwnedComponents.emplace(DupComponent);
		}
	}
	


	return DupActor;
}