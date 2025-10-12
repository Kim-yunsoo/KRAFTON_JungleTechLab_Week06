#pragma once
#include "StaticMeshComponent.h"
class UGizmoArrowComponent : public UStaticMeshComponent
{
public:
    DECLARE_CLASS(UGizmoArrowComponent, UStaticMeshComponent)
    UGizmoArrowComponent();

protected:
    ~UGizmoArrowComponent() override;

public:
    void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, FViewport* Viewport = nullptr) override;

    const FVector& GetDirection() const { return Direction; }
    const FVector& GetColor() const { return Color; }

    void SetDirection(const FVector& InDirection) { Direction = InDirection; }
    void SetColor(const FVector& InColor) { Color = InColor; }

protected:
    FVector Direction;
    FVector Color;
};

