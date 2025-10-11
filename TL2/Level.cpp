#include "pch.h"
#include "Level.h"

ULevel::ULevel()
{
}

ULevel::~ULevel()
{
	Actors.clear();
}

void ULevel::AddActor(AActor* InActor)
{	
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
