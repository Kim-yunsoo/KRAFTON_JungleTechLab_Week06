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


UDecalComponent::UDecalComponent()
{
    // 기본 큐브 메쉬 로드 (데칼 볼륨으로 사용)
    DecalBoxMesh = UResourceManager::GetInstance().Load<UStaticMesh>("Data/Cube.obj");
    // 기본 데칼 텍스처 로드

    //SetMaterial("DecalShader.hlsl");
    SetMaterial("ProjectionDecal.hlsl");
    if (Material)
    {
        //Material->Load("Editor/Decal/SpotLight_64x.dds", UResourceManager::GetInstance().GetDevice());
        Material->Load("Editor/Decal/After.png", UResourceManager::GetInstance().GetDevice());
    }

    //Decal Stat  Init
    SortOrder = 0;
    FadeInDuration = 3;
    FadeStartDelay = 3;
    FadeDuration = 3; 

    CurrentAlpha = 1.0f;
    for (int i = 0; i < 4; ++i)
    {
        CurrentStateElapsedTime[i] = 0.0f;
    }
    DecalCurrentState = EDecalState::FadeIn;
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
      
    // Prepare pipeline 
    UShader* DecalProjShader = UResourceManager::GetInstance().Load<UShader>("ProjectionDecal.hlsl");

    Renderer->PrepareShader(DecalProjShader);
    Renderer->OMSetBlendState(true);
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);  

    ID3D11DeviceContext* DevieContext = Renderer->GetRHIDevice()->GetDeviceContext();

    // Bind decal texture
    if (Material->GetTexture())
    {
        ID3D11ShaderResourceView* texSRV = Material->GetTexture()->GetShaderResourceView();
        DevieContext->PSSetShaderResources(0, 1, &texSRV);
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
        Renderer->UpdateColorBuffer(FVector4{ 1.0f, 1.0f, 1.0f, CurrentAlpha });

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
        DevieContext->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        DevieContext->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
        DevieContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        DevieContext->DrawIndexed(Mesh->GetIndexCount(), 0, 0);
    }

    // Unbind SRVs
    ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
    DevieContext->PSSetShaderResources(0, 2, nullSRV);

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

void UDecalComponent::DecalAnimTick(float DeltaTime)
{ 
    const uint8 stateIndex = static_cast<uint8>(DecalCurrentState);

    if (DecalCurrentState != EDecalState::Finished)
    {
        if (stateIndex < (uint8)4)
        {
            CurrentStateElapsedTime[stateIndex] += DeltaTime;
        }
    }

    ActivateFadeEffect();
}

void UDecalComponent::ActivateFadeEffect()
{
    switch (DecalCurrentState)
    {
    case EDecalState::FadeIn:
    {
        const float Duration = FMath::Max(0.0f, GetFadeInDuration());
        if (Duration == 0.0f)
        {
            CurrentAlpha = 1.0f;
            DecalCurrentState = EDecalState::Delay;
            CurrentStateElapsedTime[static_cast<uint8>(EDecalState::Delay)] = 0.0f;
        }
        else
        {
            CurrentAlpha = FMath::Clamp(CurrentStateElapsedTime[static_cast<uint8>(EDecalState::FadeIn)] / Duration, 0.0f, 1.0f);
            if (CurrentAlpha == 1.0f)
            {
                DecalCurrentState = EDecalState::Delay;
                CurrentStateElapsedTime[static_cast<uint8>(EDecalState::Delay)] = 0.0f; 
            }

        }
         
        break;
    }
    case EDecalState::Delay:
    {
        CurrentAlpha = 1.0f;
        if (CurrentStateElapsedTime[static_cast<uint8>(EDecalState::Delay)] > GetFadeStartDelay())
        {
            DecalCurrentState = EDecalState::FadingOut;
            CurrentStateElapsedTime[static_cast<uint8>(EDecalState::FadingOut)] = 0.0f;
        }
        break;
    }

    case EDecalState::FadingOut:
    {
        const float Duration = FMath::Max(0.0f, GetFadeDuration());
        if (Duration == 0.0f)
        {
            CurrentAlpha = 0.0f;
            DecalCurrentState = EDecalState::Finished;
        }
        else
        {
            const float InvAlpha = FMath::Clamp(CurrentStateElapsedTime[static_cast<uint8>(EDecalState::FadingOut)] / Duration, 0.0f, 1.0f);
            CurrentAlpha = 1.0f - InvAlpha;
           if (InvAlpha >= 1.0f)
            {
                DecalCurrentState = EDecalState::Finished;  
            }
        }
        break;
    }
    case EDecalState::Finished:
    {
        CurrentAlpha = 0.0f; 
        break;
    }
    }
}

void UDecalComponent::StartFade()
{
    // Reset Timers
    for (int i = 0; i < 4; ++i)
    {
        CurrentStateElapsedTime[i] = 0.0f;
    }
    CurrentAlpha = 0.0f;
    DecalCurrentState = EDecalState::FadeIn;
}

UObject* UDecalComponent::Duplicate()
{
    UDecalComponent* DuplicatedComponent = Cast<UDecalComponent>(NewObject(GetClass()));
   
    
    if (DuplicatedComponent)
    {
        DuplicatedComponent->SetFadeInDuration(GetFadeInDuration());
        DuplicatedComponent->SetFadeStartDelay(GetFadeStartDelay());
        DuplicatedComponent->SetFadeDuration(GetFadeDuration());

        //DuplicatedComponent->DecalSize = DecalSize;
        //DuplicatedComponent->BlendMode = BlendMode;
    }
    return DuplicatedComponent;
}

void UDecalComponent::DuplicateSubObjects()
{
    UPrimitiveComponent::DuplicateSubObjects();
    // DecalBoxMesh는 공유 리소스이므로 복사하지 않음
}
