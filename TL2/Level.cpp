#include "pch.h"
#include "Level.h"
#include "DecalActor.h"

ULevel::ULevel()
{
}

ULevel::~ULevel()
{
	Actors.clear();
	DecalActors.clear();
}

void ULevel::AddActor(AActor* InActor)
{	
	if (InActor && InActor->IsA<ADecalActor>())
	{
		DecalActors.Add(static_cast<ADecalActor*>(InActor));
	}

	if (InActor)	
	{
		Actors.Add(InActor);
	}
}
 
void ULevel::RemoveActor(AActor* InActor)
{
	if (InActor)
	{
		Actors.Remove(InActor);
		//delete InActor;
	}
}

const TArray<AActor*>& ULevel::GetActors() const
{
	return Actors;
}

TArray<AActor*>& ULevel::GetActors() 
{
	return Actors;
}

const TArray<ADecalActor*>& ULevel::GetDecalActors() const
{
	return DecalActors;
}
