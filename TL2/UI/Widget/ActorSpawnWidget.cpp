#include "pch.h"
#include "ActorSpawnWidget.h"
#include "UI/UIManager.h"
#include "ImGui/imgui.h"
#include "World.h"
#include "CameraActor.h"
#include "GridActor.h"
#include "GizmoActor.h"
#include "StaticMeshActor.h"
#include "DecalActor.h"
#include "StaticMeshComponent.h"
#include "ResourceManager.h"

UActorSpawnWidget::UActorSpawnWidget()
    : UWidget("Actor Spawner")
{
}

UActorSpawnWidget::~UActorSpawnWidget() = default;

void UActorSpawnWidget::Initialize()
{
}

void UActorSpawnWidget::Update()
{
}

UWorld* UActorSpawnWidget::GetCurrentWorld() const
{
    return UUIManager::GetInstance().GetWorld();
}

void UActorSpawnWidget::RenderWidget()
{
    ImGui::Text("Actor Spawner");
    ImGui::Separator();
       
    ImGui::SameLine();
    if (ImGui::Button("Spawn StaticMesh Actor", ImVec2(140, 0)))
    {
        SpawnStaticMesh();
    }

    if (ImGui::Button("Spawn Decal", ImVec2(140, 0)))
    {
        SpawnDecal();
    }
}

void UActorSpawnWidget::SpawnCamera()
{
    UWorld* World = GetCurrentWorld();
    if (!World) return;

    ACameraActor* NewActor = World->SpawnActor<ACameraActor>();
    if (NewActor)
    {
        NewActor->SetName(World->GenerateUniqueActorName("Camera").c_str());
        NewActor->SetActorLocation({0.f, -5.f, 3.f});
    }
}
 

void UActorSpawnWidget::SpawnStaticMesh()
{
    UWorld* World = GetCurrentWorld();
    if (!World) return;

    AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>();
    if (NewActor)
    {
        if (auto* SMC = NewActor->GetStaticMeshComponent())
        {
            SMC->SetStaticMesh("Data/Cube.obj");
            NewActor->SetCollisionComponent();
        }
        NewActor->SetName(World->GenerateUniqueActorName("StaticMesh").c_str());
    }
}

void UActorSpawnWidget::SpawnDecal()
{
    UWorld* World = GetCurrentWorld();
    if (!World) return;

    ADecalActor* NewActor = World->SpawnActor<ADecalActor>();
    if (NewActor)
    {
        NewActor->SetName(World->GenerateUniqueActorName("Decal").c_str());
    }
}

