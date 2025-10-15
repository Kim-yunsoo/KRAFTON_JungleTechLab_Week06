#include "pch.h"

#include "BillboardComponent.h"
#include "TextRenderComponent.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "TextQuad.h"
#include "StaticMeshComponent.h"
#include "RenderingStats.h"
#include "UI/StatsOverlayD2D.h"
#include "D3D11RHI.h"


URenderer::URenderer(URHIDevice* InDevice) : RHIDevice(InDevice)
{
    InitializeLineBatch();
}

URenderer::~URenderer()
{
    if (LineBatchData)
    {
        delete LineBatchData;
    }
}

void URenderer::BeginFrame()
{
    // 렌더링 통계 수집 시작
    URenderingStatsCollector::GetInstance().BeginFrame();
    
    // 상태 추적 리셋
    ResetRenderStateTracking();
    
    // 백버퍼/깊이버퍼를 클리어
    RHIDevice->ClearBackBuffer();  // 배경색
    RHIDevice->ClearDepthBuffer(1.0f, 0);                 // 깊이값 초기화
    RHIDevice->CreateBlendState();
    RHIDevice->IASetPrimitiveTopology();
    // RS
    RHIDevice->RSSetViewport();

    //OM
    //RHIDevice->OMSetBlendState();
    RHIDevice->OMSetRenderTargets();
	OMSetDepthStencilState(EComparisonFunc::LessEqual);
}

void URenderer::PrepareShader(FShader& InShader)
{
    RHIDevice->GetDeviceContext()->VSSetShader(InShader.SimpleVertexShader, nullptr, 0);
    RHIDevice->GetDeviceContext()->PSSetShader(InShader.SimplePixelShader, nullptr, 0);
    RHIDevice->GetDeviceContext()->IASetInputLayout(InShader.SimpleInputLayout);
}

void URenderer::PrepareShader(UShader* InShader)
{
    // 셰이더 변경 추적
    if (LastShader != InShader)
    {
        URenderingStatsCollector::GetInstance().IncrementShaderChanges();
        LastShader = InShader;
    }
    
    RHIDevice->GetDeviceContext()->VSSetShader(InShader->GetVertexShader(), nullptr, 0);
    RHIDevice->GetDeviceContext()->PSSetShader(InShader->GetPixelShader(), nullptr, 0);
    RHIDevice->GetDeviceContext()->IASetInputLayout(InShader->GetInputLayout());
}

void URenderer::OMSetBlendState(bool bIsChecked)
{
    if (bIsChecked == true)
    {
        RHIDevice->OMSetBlendState(true);
    }
    else
    {
        RHIDevice->OMSetBlendState(false);
    }
}

void URenderer::RSSetState(EViewModeIndex ViewModeIndex)
{
    RHIDevice->RSSetState(ViewModeIndex);
}

void URenderer::RSSetFrontCullState()
{
    RHIDevice->RSSetFrontCullState();
}

void URenderer::RSSetNoCullState()
{
    RHIDevice->RSSetNoCullState();
}

void URenderer::RSSetDefaultState()
{
    RHIDevice->RSSetDefaultState();
}

void URenderer::UpdateConstantBuffer(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
{
    RHIDevice->UpdateConstantBuffers(ModelMatrix, ViewMatrix, ProjMatrix);
}

void URenderer::UpdateHighLightConstantBuffer(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo)
{
    RHIDevice->UpdateHighLightConstantBuffers(InPicked, InColor, X, Y, Z, Gizmo);
}

void URenderer::UpdateBillboardConstantBuffers(const FVector& pos,const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, const FVector& CameraRight, const FVector& CameraUp)
{
    RHIDevice->UpdateBillboardConstantBuffers(pos,ViewMatrix, ProjMatrix, CameraRight, CameraUp);
}

//void URenderer::UpdateTextConstantBuffers(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
//{
//    RHIDevice->UpdateTextConstantBuffers(ModelMatrix, ViewMatrix, ProjMatrix);
//}

void URenderer::UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture)
{
    RHIDevice->UpdatePixelConstantBuffers(InMaterialInfo, bHasMaterial, bHasTexture);
}

void URenderer::UpdateColorBuffer(const FVector4& Color)
{
    RHIDevice->UpdateColorConstantBuffers(Color);
}

void URenderer::UpdateInvWorldBuffer(const FMatrix& InvWorldMatrix, const FMatrix& InvViewProjMatrix)
{
    RHIDevice->UpdateInvWorldConstantBuffer(InvWorldMatrix, InvViewProjMatrix);
}

void URenderer::UpdateViewportBuffer(float StartX, float StartY, float SizeX, float SizeY)
{
    static_cast<D3D11RHI*>(RHIDevice)->UpdateViewportConstantBuffer(StartX, StartY, SizeX, SizeY);
}

void URenderer::UpdateDepthVisualizationBuffer(float NearPlane, float FarPlane, float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight, float ScreenWidth, float ScreenHeight)
{
    static_cast<D3D11RHI*>(RHIDevice)->UpdateDepthVisualizationBuffer(
        NearPlane, FarPlane,
        ViewportX, ViewportY,
        ViewportWidth, ViewportHeight,
        ScreenWidth, ScreenHeight
    );
}

void URenderer::UpdateUVScroll(const FVector2D& Speed, float TimeSec)
{
    RHIDevice->UpdateUVScrollConstantBuffers(Speed, TimeSec);
}

void URenderer::UpdateHeatConstantBuffer(const FHeatInfo& HeatCB)
{ 
    RHIDevice->UpdateHeatConstantBuffer(HeatCB);
}

void URenderer::DrawIndexedPrimitiveComponent(UStaticMesh* InMesh, D3D11_PRIMITIVE_TOPOLOGY InTopology, const TArray<FMaterialSlot>& InComponentMaterialSlots)
{
    URenderingStatsCollector& StatsCollector = URenderingStatsCollector::GetInstance();
    
    // 디버그: StaticMesh 렌더링 통계
    
    UINT stride = 0;
    switch (InMesh->GetVertexType())
    {
    case EVertexLayoutType::PositionColor:
        stride = sizeof(FVertexSimple);
        break;
    case EVertexLayoutType::PositionColorTexturNormal:
        stride = sizeof(FVertexDynamic);
        break;
    case EVertexLayoutType::PositionBillBoard:
        stride = sizeof(FBillboardVertexInfo_GPU);
        break;
    default:
        // Handle unknown or unsupported vertex types
        assert(false && "Unknown vertex type!");
        return; // or log an error
    }
    UINT offset = 0;

    ID3D11Buffer* VertexBuffer = InMesh->GetVertexBuffer();
    ID3D11Buffer* IndexBuffer = InMesh->GetIndexBuffer();
    uint32 VertexCount = InMesh->GetVertexCount();
    uint32 IndexCount = InMesh->GetIndexCount();

    RHIDevice->GetDeviceContext()->IASetVertexBuffers(
        0, 1, &VertexBuffer, &stride, &offset
    );

    RHIDevice->GetDeviceContext()->IASetIndexBuffer(
        IndexBuffer, DXGI_FORMAT_R32_UINT, 0
    );

    RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
    RHIDevice->PSSetDefaultSampler(0);

    if (InMesh->HasMaterial())
    {
        const TArray<FGroupInfo>& MeshGroupInfos = InMesh->GetMeshGroupInfo();
        const uint32 NumMeshGroupInfos = static_cast<uint32>(MeshGroupInfos.size());
        for (uint32 i = 0; i < NumMeshGroupInfos; ++i)
        {
            const UMaterial* const Material = UResourceManager::GetInstance().Get<UMaterial>(InComponentMaterialSlots[i].MaterialName);
            const FObjMaterialInfo& MaterialInfo = Material->GetMaterialInfo();
            bool bHasTexture = !(MaterialInfo.DiffuseTextureFileName == FName::None());
            
            // 재료 변경 추적
            if (LastMaterial != Material)
            {
                StatsCollector.IncrementMaterialChanges();
                LastMaterial = const_cast<UMaterial*>(Material);
            }
            
            FTextureData* TextureData = nullptr;
            if (bHasTexture)
            {
                TextureData = UResourceManager::GetInstance().CreateOrGetTextureData(MaterialInfo.DiffuseTextureFileName);
                
                // 텍스처 변경 추적 (임시로 FTextureData*를 UTexture*로 캠스트)
                UTexture* CurrentTexture = reinterpret_cast<UTexture*>(TextureData);
                if (LastTexture != CurrentTexture)
                {
                    StatsCollector.IncrementTextureChanges();
                    LastTexture = CurrentTexture;
                }
                
                RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &(TextureData->TextureSRV));
            }
            
            RHIDevice->UpdatePixelConstantBuffers(MaterialInfo, true, bHasTexture); // PSSet도 해줌
            
            // DrawCall 수실행 및 통계 추가
            RHIDevice->GetDeviceContext()->DrawIndexed(MeshGroupInfos[i].IndexCount, MeshGroupInfos[i].StartIndex, 0);
            StatsCollector.IncrementDrawCalls();
        }
    }
    else
    {
        FObjMaterialInfo ObjMaterialInfo;
        RHIDevice->UpdatePixelConstantBuffers(ObjMaterialInfo, false, false); // PSSet도 해줌
        RHIDevice->GetDeviceContext()->DrawIndexed(IndexCount, 0, 0);
        StatsCollector.IncrementDrawCalls();
    }
}

void URenderer::DrawIndexedPrimitiveComponent(UTextRenderComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
    URenderingStatsCollector& StatsCollector = URenderingStatsCollector::GetInstance();
    
    // 디버그: TextRenderComponent 렌더링 통계
    
    UINT Stride = sizeof(FBillboardVertexInfo_GPU);
    ID3D11Buffer* VertexBuff = Comp->GetStaticMesh()->GetVertexBuffer();
    ID3D11Buffer* IndexBuff = Comp->GetStaticMesh()->GetIndexBuffer();

    // 매테리얼 변경 추적
    UMaterial* CompMaterial = Comp->GetMaterial();
    if (LastMaterial != CompMaterial)
    {
        StatsCollector.IncrementMaterialChanges();
        LastMaterial = CompMaterial;
    }
    
    UShader* CompShader = CompMaterial->GetShader();
    // 셰이더 변경 추적
    if (LastShader != CompShader)
    {
        StatsCollector.IncrementShaderChanges();
        LastShader = CompShader;
    }
    
    RHIDevice->GetDeviceContext()->IASetInputLayout(CompShader->GetInputLayout());

    
    UINT offset = 0;
    RHIDevice->GetDeviceContext()->IASetVertexBuffers(
        0, 1, &VertexBuff, &Stride, &offset
    );
    RHIDevice->GetDeviceContext()->IASetIndexBuffer(
        IndexBuff, DXGI_FORMAT_R32_UINT, 0
    );

    // 텍스처 변경 추적 (텍스처 비교)
    UTexture* CompTexture = CompMaterial->GetTexture();
    if (LastTexture != CompTexture)
    {
        StatsCollector.IncrementTextureChanges();
        LastTexture = CompTexture;
    }
    
    ID3D11ShaderResourceView* TextureSRV = CompTexture->GetShaderResourceView();
    RHIDevice->PSSetDefaultSampler(0);
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &TextureSRV);
    RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
    RHIDevice->GetDeviceContext()->DrawIndexed(Comp->GetStaticMesh()->GetIndexCount(), 0, 0);
    StatsCollector.IncrementDrawCalls();
}


void URenderer::DrawIndexedPrimitiveComponent(UBillboardComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
    URenderingStatsCollector& StatsCollector = URenderingStatsCollector::GetInstance();
    
    // 디버그: TextRenderComponent 렌더링 통계
    
    UINT Stride = sizeof(FBillboardVertexInfo_GPU);
    ID3D11Buffer* VertexBuff = Comp->GetStaticMesh()->GetVertexBuffer();
    ID3D11Buffer* IndexBuff = Comp->GetStaticMesh()->GetIndexBuffer();

    // 매테리얼 변경 추적
    UMaterial* CompMaterial = Comp->GetMaterial();
    if (LastMaterial != CompMaterial)
    {
        StatsCollector.IncrementMaterialChanges();
        LastMaterial = CompMaterial;
    }
    
    UShader* CompShader = CompMaterial->GetShader();
    // 셰이더 변경 추적
    if (LastShader != CompShader)
    {
        StatsCollector.IncrementShaderChanges();
        LastShader = CompShader;
    }
    
    RHIDevice->GetDeviceContext()->IASetInputLayout(CompShader->GetInputLayout());

    
    UINT offset = 0;
    RHIDevice->GetDeviceContext()->IASetVertexBuffers(
        0, 1, &VertexBuff, &Stride, &offset
    );
    RHIDevice->GetDeviceContext()->IASetIndexBuffer(
        IndexBuff, DXGI_FORMAT_R32_UINT, 0
    );

    // 텍스처 변경 추적 (텍스처 비교)
    UTexture* CompTexture = CompMaterial->GetTexture();
    if (LastTexture != CompTexture)
    {
        StatsCollector.IncrementTextureChanges();
        LastTexture = CompTexture;
    }
    
    ID3D11ShaderResourceView* TextureSRV = CompTexture->GetShaderResourceView();
    RHIDevice->PSSetDefaultSampler(0);
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &TextureSRV);
    RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(InTopology);
    RHIDevice->GetDeviceContext()->DrawIndexed(Comp->GetStaticMesh()->GetIndexCount(), 0, 0);
    StatsCollector.IncrementDrawCalls();
}

void URenderer::SetViewModeType(EViewModeIndex ViewModeIndex)
{
    RHIDevice->RSSetState(ViewModeIndex);

    if (ViewModeIndex == EViewModeIndex::VMI_Wireframe)
    {
        RHIDevice->UpdateColorConstantBuffers(FVector4{ 1.f, 0.f, 0.f, 1.f });
    }
    else if (ViewModeIndex == EViewModeIndex::VMI_SceneDepth)
    {
        //// Scene Depth 모드: Depth SRV를 픽셀 셰이더 t0 슬롯에 바인딩
        //ID3D11ShaderResourceView* DepthShaderResourceView = static_cast<D3D11RHI*>(RHIDevice)->GetDepthShaderResourceView();
        //if (DepthShaderResourceView)
        //{
        //    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &DepthShaderResourceView);
        //}

        //// Scene Depth 셰이더로 전환하기 위한 플래그 설정
        //RHIDevice->UpdateColorConstantBuffers(FVector4{ 1.f, 1.f, 1.f, 1.f });
        return;
    }
    else
    {
        RHIDevice->UpdateColorConstantBuffers(FVector4{ 1.f, 1.f, 1.f, 0.f });
    }
}

void URenderer::EndFrame()
{
    // 렌더링 통계 수집 종료
    URenderingStatsCollector& StatsCollector = URenderingStatsCollector::GetInstance();
    StatsCollector.EndFrame();
    
    // 현재 프레임 통계를 업데이트
    const FRenderingStats& CurrentStats = StatsCollector.GetCurrentFrameStats();
    StatsCollector.UpdateFrameStats(CurrentStats);
    
    // 평균 통계를 얻어서 오버레이에 업데이트
    const FRenderingStats& AvgStats = StatsCollector.GetAverageStats();
    UStatsOverlayD2D::Get().UpdateRenderingStats(
        AvgStats.TotalDrawCalls,
        AvgStats.MaterialChanges,
        AvgStats.TextureChanges,
        AvgStats.ShaderChanges
    ); 
    //// Post-process: FXAA fullscreen pass
    //if (!FXAAShader)
    //{
    //    FXAAShader = UResourceManager::GetInstance().Load<UShader>("FXAA.hlsl");
    //}

    //// Update viewport CB (b6) and bind backbuffer for FXAA
    //static_cast<D3D11RHI*>(RHIDevice)->UpdateViewportCBFromCurrent();
    //// Bind backbuffer as target (no depth) for FXAA output
    //static_cast<D3D11RHI*>(RHIDevice)->OMSetBackBufferNoDepth();

    //// Set FXAA shader (uses SV_VertexID, no input layout)
    //RHIDevice->GetDeviceContext()->VSSetShader(FXAAShader->GetVertexShader(), nullptr, 0);
    //RHIDevice->GetDeviceContext()->PSSetShader(FXAAShader->GetPixelShader(), nullptr, 0);
    //RHIDevice->GetDeviceContext()->IASetInputLayout(FXAAShader->GetInputLayout());
    //RHIDevice->IASetPrimitiveTopology();
    //// Ensure no blending for fullscreen resolve
    //RHIDevice->OMSetBlendState(false);

    //// Bind source color as t0
    //ID3D11ShaderResourceView* srcSRV = static_cast<D3D11RHI*>(RHIDevice)->GetFXAASRV();
    //RHIDevice->PSSetDefaultSampler(0);
    //RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &srcSRV);

    //// Draw fullscreen triangle
    //RHIDevice->GetDeviceContext()->Draw(3, 0);

    //// Unbind SRV to avoid warnings on next frame when rebinding as RTV
    //ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    //RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, nullSRV);

    //// Present is moved out to World::Render after UI pass,
    //// so that UI/overlay are not affected by FXAA.
}

void URenderer::OMSetDepthStencilState(EComparisonFunc Func)
{
    RHIDevice->OmSetDepthStencilState(Func);
}

// Lighting cache API
void URenderer::SetWorldLights(const TArray<FLightInfo>& InLights)
{
    WorldLights = InLights;
}

const TArray<FLightInfo>& URenderer::GetWorldLights() const
{
    return WorldLights;
}

void URenderer::UpdateLightBuffer()
{
    RHIDevice->UpdateLightConstantBuffers(WorldLights);
}

void URenderer::UpdateLightBuffer(const TArray<FLightInfo>& InLights)
{
    RHIDevice->UpdateLightConstantBuffers(InLights);
}

void URenderer::SetFXAAEnabled(bool bEnabled)
{ 
    bFXAAEnabled = bEnabled;
    static_cast<D3D11RHI*>(RHIDevice)->SetFXAAEnabledFlag(bEnabled); 
}

void URenderer::SetFXAAParams(float SpanMax, float ReduceMul, float ReduceMin)
{
    D3D11RHI* RHI = static_cast<D3D11RHI*>(RHIDevice);
    if (!RHI) return;

    IDXGISwapChain* SwapChain = RHI->GetSwapChain();
    if (!SwapChain) return;

    DXGI_SWAP_CHAIN_DESC swapDesc;
    SwapChain->GetDesc(&swapDesc);

    FXAAInfo info{};
    info.InvResolution[0] = 1.0f / static_cast<float>(swapDesc.BufferDesc.Width);
    info.InvResolution[1] = 1.0f / static_cast<float>(swapDesc.BufferDesc.Height);
    info.Enabled = bFXAAEnabled;

    if (bFXAAEnabled)
    {
        info.FXAASpanMax = SpanMax;
        info.FXAAReduceMul = ReduceMul;
        info.FXAAReduceMin = ReduceMin;
    }

    RHI->UpdateFXAAConstantBuffers(info);
}
 
void URenderer::EnsurePostProcessingShader()
{
    if (!HeatShader)
    {
        HeatShader = UResourceManager::GetInstance().Load<UShader>("HeatDistortion.hlsl");
    }
    if (!FXAAShader)
    {
        FXAAShader = UResourceManager::GetInstance().Load<UShader>("FXAA.hlsl");
    }
}

void URenderer::InitializeLineBatch()
{
    // Create UDynamicMesh for efficient line batching
    DynamicLineMesh = UResourceManager::GetInstance().Load<ULineDynamicMesh>("Line");
    
    // Initialize with maximum capacity (MAX_LINES * 2 vertices, MAX_LINES * 2 indices)
    uint32 maxVertices = MAX_LINES * 2;
    uint32 maxIndices = MAX_LINES * 2;
    DynamicLineMesh->Load(maxVertices, maxIndices, RHIDevice->GetDevice());

    // Create FMeshData for accumulating line data
    LineBatchData = new FMeshData();
    
    // Load line shader
    LineShader = UResourceManager::GetInstance().Load<UShader>("ShaderLine.hlsl");
}

void URenderer::BeginLineBatch()
{
    if (!LineBatchData) return;
    
    bLineBatchActive = true;
    
    // Clear previous batch data
    LineBatchData->Vertices.clear();
    LineBatchData->Color.clear();
    LineBatchData->Indices.clear();
}

void URenderer::AddLine(const FVector& Start, const FVector& End, const FVector4& Color)
{
    if (!bLineBatchActive || !LineBatchData) return;
    
    uint32 startIndex = static_cast<uint32>(LineBatchData->Vertices.size());
    
    // Add vertices
    LineBatchData->Vertices.push_back(Start);
    LineBatchData->Vertices.push_back(End);
    
    // Add colors
    LineBatchData->Color.push_back(Color);
    LineBatchData->Color.push_back(Color);
    
    // Add indices for line (2 vertices per line)
    LineBatchData->Indices.push_back(startIndex);
    LineBatchData->Indices.push_back(startIndex + 1);
}

void URenderer::AddLines(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors)
{
    if (!bLineBatchActive || !LineBatchData) return;
    
    // Validate input arrays have same size
    if (StartPoints.size() != EndPoints.size() || StartPoints.size() != Colors.size())
        return;
    
    uint32 startIndex = static_cast<uint32>(LineBatchData->Vertices.size());
    
    // Reserve space for efficiency
    size_t lineCount = StartPoints.size();
    LineBatchData->Vertices.reserve(LineBatchData->Vertices.size() + lineCount * 2);
    LineBatchData->Color.reserve(LineBatchData->Color.size() + lineCount * 2);
    LineBatchData->Indices.reserve(LineBatchData->Indices.size() + lineCount * 2);
    
    // Add all lines at once
    for (size_t i = 0; i < lineCount; ++i)
    {
        uint32 currentIndex = startIndex + static_cast<uint32>(i * 2);
        
        // Add vertices
        LineBatchData->Vertices.push_back(StartPoints[i]);
        LineBatchData->Vertices.push_back(EndPoints[i]);
        
        // Add colors
        LineBatchData->Color.push_back(Colors[i]);
        LineBatchData->Color.push_back(Colors[i]);
        
        // Add indices for line (2 vertices per line)
        LineBatchData->Indices.push_back(currentIndex);
        LineBatchData->Indices.push_back(currentIndex + 1);
    }
}

void URenderer::EndLineBatch(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
    if (!bLineBatchActive || !LineBatchData || !DynamicLineMesh || LineBatchData->Vertices.empty())
    {
        bLineBatchActive = false;
        return;
    }
    
    // Efficiently update dynamic mesh data (no buffer recreation!)
    if (!DynamicLineMesh->UpdateData(LineBatchData, RHIDevice->GetDeviceContext()))
    {
        bLineBatchActive = false;
        return;
    } 
    // Set up rendering state
    UpdateConstantBuffer(ModelMatrix, ViewMatrix, ProjectionMatrix);
    PrepareShader(LineShader);
    
    // Render using dynamic mesh
    if (DynamicLineMesh->GetCurrentVertexCount() > 0 && DynamicLineMesh->GetCurrentIndexCount() > 0)
    {
        UINT stride = sizeof(FVertexSimple);
        UINT offset = 0;
        
        ID3D11Buffer* vertexBuffer = DynamicLineMesh->GetVertexBuffer();
        ID3D11Buffer* indexBuffer = DynamicLineMesh->GetIndexBuffer();
        
        RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        RHIDevice->GetDeviceContext()->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        RHIDevice->GetDeviceContext()->DrawIndexed(DynamicLineMesh->GetCurrentIndexCount(), 0, 0);
        
        // 라인 렌더링에 대한 DrawCall 통계 추가
        URenderingStatsCollector::GetInstance().IncrementDrawCalls();
    }
    
    bLineBatchActive = false;
}

void URenderer::ResetRenderStateTracking()
{
    LastMaterial = nullptr;
    LastShader = nullptr;
    LastTexture = nullptr;
}

void URenderer::ClearLineBatch()
{
    if (!LineBatchData) return;
    
    LineBatchData->Vertices.clear();
    LineBatchData->Color.clear();
    LineBatchData->Indices.clear();
    
    bLineBatchActive = false;
}


