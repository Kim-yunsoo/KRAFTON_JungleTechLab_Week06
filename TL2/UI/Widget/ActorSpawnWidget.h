#pragma once
#include "Widget.h"

class UWorld;

class UActorSpawnWidget : public UWidget
{
public:
    DECLARE_CLASS(UActorSpawnWidget, UWidget)

    UActorSpawnWidget();
    ~UActorSpawnWidget() override;

    void Initialize() override;
    void Update() override;
    void RenderWidget() override;

private:
    UWorld* GetCurrentWorld() const;
    void SpawnCamera(); 
    void SpawnStaticMesh();
    void SpawnDecal();
};

