#include "pch.h"
#include "GizmoArrowComponent.h"

UGizmoArrowComponent::UGizmoArrowComponent()
{
    SetStaticMesh("Data/Arrow.obj");
    SetMaterial("Primitive.hlsl");
}

UGizmoArrowComponent::~UGizmoArrowComponent()
{

}

void UGizmoArrowComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, FViewport* Viewport)
{
    if (!StaticMesh || !Renderer)
    {
        return;
    }

    // 1. 메쉬 렌더링 (항상 실행)
    Renderer->UpdateConstantBuffer(GetWorldMatrix(), View, Proj);
    Renderer->PrepareShader(GetMaterial()->GetShader());
    Renderer->DrawIndexedPrimitiveComponent(GetStaticMesh(), D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST, MaterailSlots);
}
