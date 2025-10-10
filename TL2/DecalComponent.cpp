// DecalComponent.cpp
#include "pch.h"
#include "DecalComponent.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "SViewportWindow.h"

IMPLEMENT_CLASS(UDecalComponent)

UDecalComponent::UDecalComponent()
{
    // 기본 큐브 메쉬 로드 (데칼 볼륨으로 사용)
    DecalBoxMesh = UResourceManager::GetInstance().Load<UStaticMesh>("Data/Cube.obj");
    // 기본 데칼 텍스처 로드

    SetMaterial("DecalShader.hlsl");
    if (Material)
    {
        Material->Load("Editor/Decal/SpotLight_64x.dds", UResourceManager::GetInstance().GetDevice());
    }
  
}

UDecalComponent::~UDecalComponent()
{
} 
//void UDecalComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
//{
//    if (!DecalBoxMesh || !Material || !Renderer)
//        return;
//
//    // 메인 뷰포트 참조 (뷰포트 버퍼/깊이 SRV 설정에 필요)
//    UWorld* World = GetWorld();
//    if (!World)
//        return;
//    SViewportWindow* MainViewportWindow = World->GetMainViewport();
//    if (!MainViewportWindow)
//        return;
//    FViewport* Viewport = MainViewportWindow->GetViewport();
//    if (!Viewport)
//        return;
//
//    // 1) 월드/역월드 행렬 (데칼 크기를 스케일로 반영)
//    FMatrix ScaleMatrix = FMatrix::CreateScale(DecalSize);
//    FMatrix WorldMatrix = ScaleMatrix * GetWorldMatrix();
//    FMatrix InvWorldMatrix = WorldMatrix.InverseAffine();
//
//    // 2) ViewProj 및 역행렬 (투영 포함 → 일반 Inverse)
//    FMatrix ViewProj = View * Proj;
//    FMatrix InvViewProj = ViewProj.Inverse();
//
//    // 3) 상수 버퍼 업데이트
//    Renderer->UpdateConstantBuffer(WorldMatrix, View, Proj);
//    Renderer->UpdateInvWorldBuffer(InvWorldMatrix, InvViewProj);
//    Renderer->UpdateViewportBuffer(
//        static_cast<float>(Viewport->GetStartX()),
//        static_cast<float>(Viewport->GetStartY()),
//        static_cast<float>(Viewport->GetSizeX()),
//        static_cast<float>(Viewport->GetSizeY())
//    );
//
//    // 4) 셰이더/블렌딩 상태 준비
//    Renderer->PrepareShader(Material->GetShader());
//    Renderer->OMSetBlendState(true);
//
//    // 5) RTV 유지 + DSV 언바인드 (깊이 SRV 사용을 위해)
//    ID3D11RenderTargetView* currentRTV = nullptr;
//    ID3D11DeviceContext* ctx = Renderer->GetRHIDevice()->GetDeviceContext();
//    ctx->OMGetRenderTargets(1, &currentRTV, nullptr);
//    ctx->OMSetRenderTargets(1, &currentRTV, nullptr);
//    if (currentRTV) currentRTV->Release();
//
//    // 6) 깊이 읽기 전용, 프론트 컬링(볼륨 렌더링)
//    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);
//    Renderer->RSSetFrontCullState();
//
//    // 7) IA 설정 (데칼 볼륨 메시)
//    UINT stride = sizeof(FVertexDynamic);
//    UINT offset = 0;
//    ID3D11Buffer* vb = DecalBoxMesh->GetVertexBuffer();
//    ID3D11Buffer* ib = DecalBoxMesh->GetIndexBuffer();
//    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
//    ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
//    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//    // 8) 머티리얼 텍스처 + 샘플러
//    if (Material->GetTexture())
//    {
//        ID3D11ShaderResourceView* texSRV = Material->GetTexture()->GetShaderResourceView();
//        ctx->PSSetShaderResources(0, 1, &texSRV);
//    }
//    Renderer->GetRHIDevice()->PSSetDefaultSampler(0);
//
//    // 9) 깊이 SRV 바인딩(t1)
//    ID3D11ShaderResourceView* depthSRV = static_cast<D3D11RHI*>(Renderer->GetRHIDevice())->GetDepthSRV();
//    ctx->PSSetShaderResources(1, 1, &depthSRV);
//
//    // 10) 드로우
//    ctx->DrawIndexed(DecalBoxMesh->GetIndexCount(), 0, 0);
//
//    // 11) SRV 언바인드 (리소스 hazard 방지)
//    ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
//    ctx->PSSetShaderResources(0, 2, nullSRV);
//
//    // 12) 원래 상태 복원
//    Renderer->GetRHIDevice()->OMSetRenderTargets();
//    Renderer->OMSetBlendState(false);
//    Renderer->RSSetDefaultState();
//    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual);
//} 
 
 
void UDecalComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj,FViewport* Viewport)
{
    if (!DecalBoxMesh || !Material)
        return;

    // 월드/역월드
    // DecalSize를 스케일로 적용
    FMatrix ScaleMatrix = FMatrix::CreateScale(DecalSize);
    FMatrix WorldMatrix = ScaleMatrix * GetWorldMatrix();
    FMatrix InvWorldMatrix = WorldMatrix.InverseAffine(); // OK(Affine)

    // ViewProj 및 역행렬 (투영 포함 → 일반 Inverse 필요)
    FMatrix ViewProj = View * Proj;                   // row-major 기준
    FMatrix InvViewProj = ViewProj.Inverse();         // 투영 포함되므로 일반 Inverse 사용

    // 상수 버퍼 업데이트
    Renderer->UpdateConstantBuffer(WorldMatrix, View, Proj);
    Renderer->UpdateInvWorldBuffer(InvWorldMatrix, InvViewProj);

    // 뷰포트 정보 전달 (4분할 뷰포트 지원)
    Renderer->UpdateViewportBuffer(
        static_cast<float>(Viewport->GetStartX()),
        static_cast<float>(Viewport->GetStartY()),
        static_cast<float>(Viewport->GetSizeX()),
        static_cast<float>(Viewport->GetSizeY())
    );

    // 셰이더/블렌드 셋업
    Renderer->PrepareShader(Material->GetShader());
    Renderer->OMSetBlendState(true);                  // (SrcAlpha, InvSrcAlpha)인지 내부 확인

    // =========================
    // RTV 유지 + DSV 언바인드
    // =========================
    // FIX: 현재 RTV를 조회해서 DSV만 떼고 다시 바인딩
    ID3D11RenderTargetView* currentRTV = nullptr;
    ID3D11DeviceContext* ctx = Renderer->GetRHIDevice()->GetDeviceContext();

    ctx->OMGetRenderTargets(1, &currentRTV, nullptr);            // 현재 RTV 핸들 얻고
    ctx->OMSetRenderTargets(1, &currentRTV, nullptr);            // RTV 유지 + DSV 해제
    if (currentRTV) currentRTV->Release();                       // 로컬 ref release

    // 데칼은 깊이 "읽기"만 (LessEqual + DepthWrite Off)
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);

    // 컬링 끄기(양면)
    Renderer->RSSetFrontCullState();

    // 입력 어셈블러
    UINT stride = sizeof(FVertexDynamic);
    UINT offset = 0;
    ID3D11Buffer* vb = DecalBoxMesh->GetVertexBuffer();
    ID3D11Buffer* ib = DecalBoxMesh->GetIndexBuffer();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 텍스처 & 샘플러
    if (Material->GetTexture())
    {
        ID3D11ShaderResourceView* texSRV = Material->GetTexture()->GetShaderResourceView();
        ctx->PSSetShaderResources(0, 1, &texSRV);
    }
    Renderer->GetRHIDevice()->PSSetDefaultSampler(0);

    // Depth SRV 바인딩 (t1)
    ID3D11ShaderResourceView* depthSRV =
        static_cast<D3D11RHI*>(Renderer->GetRHIDevice())->GetDepthSRV();
    ctx->PSSetShaderResources(1, 1, &depthSRV);

    // 드로우
    ctx->DrawIndexed(DecalBoxMesh->GetIndexCount(), 0, 0);

    // SRV 언바인드 (리소스 hazard 방지)
    ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
    ctx->PSSetShaderResources(0, 2, nullSRV);

    // 원래 DSV/RTV 복원 (렌더러가 백버퍼/DSV 재바인딩)
    Renderer->GetRHIDevice()->OMSetRenderTargets();

    // 상태 복원
    Renderer->OMSetBlendState(false);
    Renderer->RSSetDefaultState();
    Renderer->OMSetDepthStencilState(EComparisonFunc::LessEqual); // 기본 상태로 복원
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
