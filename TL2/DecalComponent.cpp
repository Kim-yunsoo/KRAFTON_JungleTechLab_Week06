// DecalComponent.cpp
#include "pch.h"
#include "DecalComponent.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "SViewportWindow.h"
#include "D3D11RHI.h"
#include "Actor.h"
#include "StaticMeshComponent.h"
#include "StaticMesh.h"

IMPLEMENT_CLASS(UDecalComponent)

UDecalComponent::UDecalComponent()
{
    // 기본 큐브 메쉬 로드 (데칼 볼륨으로 사용)
    DecalBoxMesh = UResourceManager::GetInstance().Load<UStaticMesh>("Data/Cube.obj");
    // 기본 데칼 텍스처 로드

    //SetMaterial("DecalShader.hlsl");
    SetMaterial("ProjectionDecal.hlsl");
    if (Material)
    {
        Material->Load("Editor/Decal/SpotLight_64x.dds", UResourceManager::GetInstance().GetDevice());
    }
  
}

UDecalComponent::~UDecalComponent()
{

}  
  
void UDecalComponent::RenderOnActor(URenderer* Renderer, AActor* TargetActor, const FMatrix& View, const FMatrix& Proj)
{
    if (!Renderer || !TargetActor || !Material)
        return;

    // Decal View/Proj from this component
    FMatrix DecalView = GetWorldTransform().ToMatrixWithScaleLocalXYZ().InverseAffine();

    FVector Scale = GetRelativeScale();

    const float OrthoWidth = Scale.Y;
    const float OrthoHeight = Scale.Z;
    const float NearZ = -0.5f * Scale.X;
    const float FarZ = 0.5f * Scale.X;

    FMatrix DecalProj = FMatrix::OrthoLH(OrthoWidth, OrthoHeight, NearZ, FarZ);
      
    // Prepare pipeline with projection decal shader
    UShader* DecalProjShader = UResourceManager::GetInstance().Load<UShader>("ProjectionDecal.hlsl");

    Renderer->PrepareShader(DecalProjShader);
    Renderer->OMSetBlendState(true);
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);  

    ID3D11DeviceContext* ctx = Renderer->GetRHIDevice()->GetDeviceContext();

    // Bind decal texture
    if (Material->GetTexture())
    {
        ID3D11ShaderResourceView* texSRV = Material->GetTexture()->GetShaderResourceView();
        ctx->PSSetShaderResources(0, 1, &texSRV);
    }
    Renderer->GetRHIDevice()->PSSetDefaultSampler(0);

    // Draw each static mesh component of the target actor
    for (UActorComponent* Comp : TargetActor->GetComponents())
    {
        UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp);
        if (!SMC) continue;
        UStaticMesh* Mesh = SMC->GetStaticMesh();
        if (!Mesh) continue;

        // Per-mesh constant buffers
        Renderer->UpdateConstantBuffer(SMC->GetWorldMatrix(), View, Proj);
        // Reuse b4 to carry decal view/proj
        Renderer->UpdateInvWorldBuffer(DecalView, DecalProj);

        UINT stride = 0;
        switch (Mesh->GetVertexType())
        {
        case EVertexLayoutType::PositionColor: stride = sizeof(FVertexSimple); break;
        case EVertexLayoutType::PositionColorTexturNormal: stride = sizeof(FVertexDynamic); break;
        case EVertexLayoutType::PositionBillBoard: stride = sizeof(FBillboardVertexInfo_GPU); break;
        default: continue;
        }
        UINT offset = 0;
        ID3D11Buffer* vb = Mesh->GetVertexBuffer();
        ID3D11Buffer* ib = Mesh->GetIndexBuffer();
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ctx->DrawIndexed(Mesh->GetIndexCount(), 0, 0);
    }

    // Unbind SRVs
    ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
    ctx->PSSetShaderResources(0, 2, nullSRV);

    Renderer->OMSetBlendState(false);
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual);
}
void UDecalComponent::SetDecalTexture(const FString& TexturePath)
{
    if (!Material)
        return;

    // TextRenderComponent와 동일한 방식으로 텍스처 로드
    Material->Load(TexturePath, UResourceManager::GetInstance().GetDevice());
}

UObject* UDecalComponent::Duplicate()
{
    UDecalComponent* DuplicatedComponent = Cast<UDecalComponent>(NewObject(GetClass()));
    if (DuplicatedComponent)
    {
        DuplicatedComponent->DecalSize = DecalSize;
        DuplicatedComponent->BlendMode = BlendMode;
    }
    return DuplicatedComponent;
}

void UDecalComponent::DuplicateSubObjects()
{
    UPrimitiveComponent::DuplicateSubObjects();
    // DecalBoxMesh는 공유 리소스이므로 복사하지 않음
}
