#include "pch.h"
#include "FireBallActor.h"
#include "PointLightComponent.h"
#include "StaticMeshComponent.h"
#include "World.h"
#include "Renderer.h"

AFireBallActor::AFireBallActor()
{
    // Add Point Light Component
    PointLightComponent = CreateDefaultSubobject<UPointLightComponent>("PointLight");
    PointLightComponent->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);  

    // Apply procedural fireball surface shader to the mesh
    if (UStaticMeshComponent* SMC = GetStaticMeshComponent())
    {
        SMC->SetMaterial("Fireball.hlsl");
    } 
}

AFireBallActor::~AFireBallActor()
{
	if (PointLightComponent)
	{
		ObjectFactory::DeleteObject(PointLightComponent);
	}
	PointLightComponent = nullptr;
}

void AFireBallActor::ClearDefaultComponents()
{
	// 부모의 ClearDefaultComponents 호출 (StaticMeshComponent 삭제)
	Super_t::ClearDefaultComponents();

	// 생성자가 만든 PointLightComponent 삭제
	if (PointLightComponent)
	{
		OwnedComponents.Remove(PointLightComponent);
		ObjectFactory::DeleteObject(PointLightComponent);
		PointLightComponent = nullptr;
	}
}

UObject* AFireBallActor::Duplicate(FObjectDuplicationParameters Parameters)
{
	auto DupActor = static_cast<AFireBallActor*>(Super_t::Duplicate(Parameters));

	UPointLightComponent* NewPointLight = nullptr;
	if (PointLightComponent)
	{
		if (auto It = Parameters.CreatedObjects.find(PointLightComponent);  It != Parameters.CreatedObjects.end())
		{
			NewPointLight = Cast<UPointLightComponent>(It->second);
		}
	}
	if (!NewPointLight)
	{
		for (UActorComponent* Comp : DupActor->OwnedComponents)
		{
			if (auto* PointLightComp = Cast<UPointLightComponent>(Comp))
			{
				NewPointLight = PointLightComp;
			}
		}
	}
	   

	TArray<UActorComponent*> ToRemove;
	for (UActorComponent* Comp : DupActor->OwnedComponents)
	{
		if (auto* PL = Cast<UPointLightComponent>(Comp))
		{
			if (PL != NewPointLight)
			{
				ToRemove.Add(PL);
			}
		}
	}
	for (UActorComponent* Comp : ToRemove)
	{
		DupActor->OwnedComponents.Remove(Comp);
		ObjectFactory::DeleteObject(Comp);
	}

	DupActor->PointLightComponent = NewPointLight;
	//TODO 부모 설정 

	return DupActor;
}

void AFireBallActor::Tick(float DeltaTime)
{
    Super_t::Tick(DeltaTime);

    // Feed time to shaders using the existing UV scroll CB (b5)
    if (World && World->GetRenderer())
    {
        // Speed zero so other materials with UVScrollSpeed=(0,0) remain unaffected
        World->GetRenderer()->UpdateUVScroll({0.0f, 0.0f}, World->GetTimeSeconds());
    }
}
