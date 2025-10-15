#include "pch.h"
#include "UI/StatsOverlayD2D.h"

struct FConstants
{
    FVector WorldPosition;
    float Scale;
};
// b0 in VS
struct ModelBufferType
{
    FMatrix Model;
};

// b0 in PS
struct FMaterialInPs
{
    FVector DiffuseColor; // Kd
    float OpticalDensity; // Ni

    FVector AmbientColor; // Ka
    float Transparency; // Tr Or d

    FVector SpecularColor; // Ks
    float SpecularExponent; // Ns

    FVector EmissiveColor; // Ke
    uint32 IlluminationModel; // illum. Default illumination model to Phong for non-Pbr materials

    FVector TransmissionFilter; // Tf
    float dummy; // 4 bytes padding
};

struct FPixelConstBufferType
{
    FMaterialInPs Material;
    bool bHasMaterial; // 1 bytes
    bool Dummy[3]; // 3 bytes padding
    bool bHasTexture; // 1 bytes
    bool Dummy2[11]; // 11 bytes padding
};

static_assert(sizeof(FPixelConstBufferType) % 16 == 0, "PixelConstData size mismatch!");

// b1
struct ViewProjBufferType
{
    FMatrix View;
    FMatrix Proj;
};

// b2
struct HighLightBufferType
{
    uint32 Picked;
    FVector Color;
    uint32 X;
    uint32 Y;
    uint32 Z;
    uint32 Gizmo;
};

struct ColorBufferType
{
    FVector4 Color;
};

// b5: Viewport 정보 (데칼용 Screen-Space UV 계산)
struct ViewportBufferType
{
    FVector4 ViewportRect; // x=StartX, y=StartY, z=SizeX, w=SizeY
};

struct BillboardBufferType
{
    FVector pos;
    FMatrix View;
    FMatrix Proj;
    FMatrix InverseViewMat;
    /*FVector cameraRight;
    FVector cameraUp;*/
};

void D3D11RHI::Initialize(HWND hWindow)
{
    // 이곳에서 Device, DeviceContext, viewport, swapchain를 초기화한다
    CreateDeviceAndSwapChain(hWindow);
    CreateFrameBuffer();
	CreateDepthBuffer();
	CreateSceneRenderTarget();
    CreateRasterizerState();
    CreateBlendState();
    CreateConstantBuffer();
	CreateDepthStencilState();
	CreateSamplerState();
    UResourceManager::GetInstance().Initialize(Device,DeviceContext);

    // Initialize Direct2D overlay after device/swapchain ready
    UStatsOverlayD2D::Get().Initialize(Device, DeviceContext, SwapChain);

    // Initialize FXAA constants once swapchain is ready
    RefreshFXAAConstantsFromSwapchain();
}

void D3D11RHI::Release()
{
    if (DeviceContext)
    {
        // 파이프라인에서 바인딩된 상태/리소스를 명시적으로 해제
        DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        DeviceContext->OMSetDepthStencilState(nullptr, 0);
        DeviceContext->RSSetState(nullptr);
        DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

        DeviceContext->ClearState();
        DeviceContext->Flush();
    }

    ReleaseSamplerState();

    // 상수버퍼
    if (HighLightCB) { HighLightCB->Release(); HighLightCB = nullptr; }
    if (ModelCB) { ModelCB->Release(); ModelCB = nullptr; }
    if (ColorCB) { ColorCB->Release(); ColorCB = nullptr; }
    if (ViewProjCB) { ViewProjCB->Release(); ViewProjCB = nullptr; }
    if (BillboardCB) { BillboardCB->Release(); BillboardCB = nullptr; }
    if (PixelConstCB) { PixelConstCB->Release(); PixelConstCB = nullptr; }
    if (LightCB) { LightCB->Release(); LightCB = nullptr; }
    if (UVScrollCB) { UVScrollCB->Release(); UVScrollCB = nullptr; }
    if (InvWorldCB) { InvWorldCB->Release(); InvWorldCB = nullptr; }
    if (ViewportCB) { ViewportCB->Release(); ViewportCB = nullptr; }
    if (ConstantBuffer) { ConstantBuffer->Release(); ConstantBuffer = nullptr; }
    if (FXAACB) { FXAACB->Release(); FXAACB = nullptr; }
    if (DepthVisualizationCB) { DepthVisualizationCB->Release(); DepthVisualizationCB = nullptr; }
	if (CameraNearFarCB) { CameraNearFarCB->Release(); CameraNearFarCB = nullptr; }
	if (FogParameterCB) { FogParameterCB->Release(); FogParameterCB = nullptr; }
	if (InvViewProjCB) { InvViewProjCB->Release(); InvViewProjCB = nullptr; }
	if (ViewportFogCB) { ViewportFogCB->Release(); ViewportFogCB = nullptr; }
	if (CopyShaderViewportCB) { CopyShaderViewportCB->Release(); CopyShaderViewportCB = nullptr; }

    // 상태 객체
    if (DepthStencilState) { DepthStencilState->Release(); DepthStencilState = nullptr; }
    if (DepthStencilStateLessEqualWrite) { DepthStencilStateLessEqualWrite->Release(); DepthStencilStateLessEqualWrite = nullptr; }
    if (DepthStencilStateLessEqualReadOnly) { DepthStencilStateLessEqualReadOnly->Release(); DepthStencilStateLessEqualReadOnly = nullptr; }
    if (DepthStencilStateAlwaysNoWrite) { DepthStencilStateAlwaysNoWrite->Release(); DepthStencilStateAlwaysNoWrite = nullptr; }
    if (DepthStencilStateDisable) { DepthStencilStateDisable->Release(); DepthStencilStateDisable = nullptr; }
    if (DepthStencilStateGreaterEqualWrite) { DepthStencilStateGreaterEqualWrite->Release(); DepthStencilStateGreaterEqualWrite = nullptr; }

    if (DefaultRasterizerState) { DefaultRasterizerState->Release();   DefaultRasterizerState = nullptr; }
    if (WireFrameRasterizerState) { WireFrameRasterizerState->Release();   WireFrameRasterizerState = nullptr; }
    if (NoCullRasterizerState) { NoCullRasterizerState->Release();   NoCullRasterizerState = nullptr; }
    if (FrontCullRasterizerState) { FrontCullRasterizerState->Release();   FrontCullRasterizerState = nullptr; }
    if (BlendState) { BlendState->Release();        BlendState = nullptr; }

    ReleaseFrameBuffer();
	ReleaseDepthBuffer();
	ReleaseSceneRenderTarget();

    // Device + SwapChain
    ReleaseDeviceAndSwapChain();
}

void D3D11RHI::ClearBackBuffer()
{
    float ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
    DeviceContext->ClearRenderTargetView(RenderTargetView, ClearColor);
    DeviceContext->ClearRenderTargetView(FXAARTV, ClearColor);
}

void D3D11RHI::ClearDepthBuffer(float Depth, UINT Stencil)
{
    DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, Depth, Stencil);

}

void D3D11RHI::ClearSceneRenderTarget()
{
    float ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	DeviceContext->ClearRenderTargetView(SceneRenderTargetView, ClearColor);
}

void D3D11RHI::CreateBlendState()
{
    // Create once; reuse every frame
    if (BlendState)
        return;

    D3D11_BLEND_DESC bd = {};
    auto& rt = bd.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;      // 스트레이트 알파
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;  // (프리멀티면 ONE / INV_SRC_ALPHA)
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_ZERO;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Device->CreateBlendState(&bd, &BlendState);
}

void D3D11RHI::CreateDepthStencilState()
{
    D3D11_DEPTH_STENCIL_DESC desc = {};
    desc.StencilEnable = FALSE;

    // 1) 기본: LessEqual + Write ALL
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    Device->CreateDepthStencilState(&desc, &DepthStencilStateLessEqualWrite);

    // 2) ReadOnly: LessEqual + Write ZERO
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    Device->CreateDepthStencilState(&desc, &DepthStencilStateLessEqualReadOnly);

    // 3) AlwaysNoWrite: Always + Write ZERO (기즈모/오버레이 용)
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    // DepthEnable은 TRUE 유지 (읽기 의미는 없지만 상태 일관성을 위해)
    Device->CreateDepthStencilState(&desc, &DepthStencilStateAlwaysNoWrite);

    // 4) Disable: DepthEnable FALSE (테스트/쓰기 모두 무시)
    desc.DepthEnable = FALSE;
    // DepthWriteMask/Func는 무시되지만 값은 그대로 둬도 됨
    Device->CreateDepthStencilState(&desc, &DepthStencilStateDisable);

    // 5) (선택) GreaterEqual + Write ALL
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
    Device->CreateDepthStencilState(&desc, &DepthStencilStateGreaterEqualWrite);
}

void D3D11RHI::CreateSamplerState()
{
    D3D11_SAMPLER_DESC SampleDesc = {};
    SampleDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampleDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SampleDesc.MinLOD = 0;
    SampleDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT HR = Device->CreateSamplerState(&SampleDesc, &DefaultSamplerState);
}

HRESULT D3D11RHI::CreateIndexBuffer(ID3D11Device* device, const FMeshData* meshData, ID3D11Buffer** outBuffer)
{
    if (!meshData || meshData->Indices.empty())
        return E_FAIL;

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = static_cast<UINT>(sizeof(uint32) * meshData->Indices.size());
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA iinitData = {};
    iinitData.pSysMem = meshData->Indices.data();

    return device->CreateBuffer(&ibd, &iinitData, outBuffer);
}

HRESULT D3D11RHI::CreateIndexBuffer(ID3D11Device* device, const FStaticMesh* mesh, ID3D11Buffer** outBuffer)
{
    if (!mesh || mesh->Indices.empty())
        return E_FAIL;

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = static_cast<UINT>(sizeof(uint32) * mesh->Indices.size());
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA iinitData = {};
    iinitData.pSysMem = mesh->Indices.data();

    return device->CreateBuffer(&ibd, &iinitData, outBuffer);
}

//이거 두개를 나눔
void D3D11RHI::UpdateConstantBuffers(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
{
   
    UpdateModelConstantBuffers(ModelMatrix);
   
    UpdateViewConstantBuffers(ViewMatrix, ProjMatrix);
    
 
}

void D3D11RHI::UpdateViewConstantBuffers(const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
{
    static FMatrix LastViewMatrix;
    static FMatrix LastProjectionMatrix;
    if (LastViewMatrix != ViewMatrix || LastProjectionMatrix != ProjMatrix)
    {
        LastViewMatrix = ViewMatrix;
        LastProjectionMatrix = ProjMatrix;
        D3D11_MAPPED_SUBRESOURCE mapped;
        DeviceContext->Map(ViewProjCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* dataPtr = reinterpret_cast<ViewProjBufferType*>(mapped.pData);

        dataPtr->View = ViewMatrix;
        dataPtr->Proj = ProjMatrix;

        DeviceContext->Unmap(ViewProjCB, 0);
        DeviceContext->VSSetConstantBuffers(1, 1, &ViewProjCB); // b1 슬롯
       
    }
}

void D3D11RHI::UpdateModelConstantBuffers(const FMatrix& ModelMatrix)
{
    // b0 : 모델 행렬
    {

        D3D11_MAPPED_SUBRESOURCE mapped;
        DeviceContext->Map(ModelCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* dataPtr = reinterpret_cast<ModelBufferType*>(mapped.pData);

        // HLSL 기본 row-major와 맞추기 위해 전치
        dataPtr->Model = ModelMatrix;

        DeviceContext->Unmap(ModelCB, 0);
        DeviceContext->VSSetConstantBuffers(0, 1, &ModelCB); // b0 슬롯
    }
}

void D3D11RHI::UpdateBillboardConstantBuffers(const FVector& pos, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix,
    const FVector& CameraRight, const FVector& CameraUp)
{
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    DeviceContext->Map(BillboardCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    auto* dataPtr = reinterpret_cast<BillboardBufferType*>(mapped.pData);

    // HLSL 기본 row-major와 맞추기 위해 전치
    dataPtr->pos = pos;
    dataPtr->View = ViewMatrix;
    dataPtr->Proj = ProjMatrix;
    dataPtr->InverseViewMat = ViewMatrix.InverseAffine();
    //dataPtr->cameraRight = CameraRight;
    //dataPtr->cameraUp = CameraUp;

    DeviceContext->Unmap(BillboardCB, 0);
    DeviceContext->VSSetConstantBuffers(0, 1, &BillboardCB); // b0 슬롯
}

void D3D11RHI::UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    DeviceContext->Map(PixelConstCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    FPixelConstBufferType* dataPtr = reinterpret_cast<FPixelConstBufferType*>(mapped.pData);

    // 이후 다양한 material들이 맵핑될 수도 있음.
    dataPtr->bHasMaterial = bHasMaterial;
    dataPtr->bHasTexture = bHasTexture;
    dataPtr->Material.DiffuseColor = InMaterialInfo.DiffuseColor;
    dataPtr->Material.AmbientColor = InMaterialInfo.AmbientColor;

    DeviceContext->Unmap(PixelConstCB, 0);
    DeviceContext->PSSetConstantBuffers(4, 1, &PixelConstCB); // b4 슬롯
}

void D3D11RHI::UpdateHighLightConstantBuffers(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo)
{
    // b2 : 색 강조
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        DeviceContext->Map(HighLightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* dataPtr = reinterpret_cast<HighLightBufferType*>(mapped.pData);

        dataPtr->Picked = InPicked;
        dataPtr->Color = InColor;
        dataPtr->X = X;
        dataPtr->Y = Y;
        dataPtr->Z = Z;
        dataPtr->Gizmo = Gizmo;
        DeviceContext->Unmap(HighLightCB, 0);
        DeviceContext->VSSetConstantBuffers(2, 1, &HighLightCB); // b2 슬롯
        DeviceContext->PSSetConstantBuffers(2, 1, &HighLightCB); // b2 슬롯 (Pixel Shader에도 바인딩)
    }
}

void D3D11RHI::UpdateColorConstantBuffers(const FVector4& InColor)
{
    // b3 : 색 설정
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        DeviceContext->Map(ColorCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* dataPtr = reinterpret_cast<ColorBufferType*>(mapped.pData);
        {
            dataPtr->Color = InColor;
        }
        DeviceContext->Unmap(ColorCB, 0);
        DeviceContext->PSSetConstantBuffers(3, 1, &ColorCB); // b3 슬롯
    }
}

void D3D11RHI::IASetPrimitiveTopology()
{
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D3D11RHI::RSSetState(EViewModeIndex ViewModeIndex)
{
    if (ViewModeIndex == EViewModeIndex::VMI_Wireframe)
    {
        DeviceContext->RSSetState(WireFrameRasterizerState);
    }
    else
    {
        DeviceContext->RSSetState(DefaultRasterizerState);
    }
}

void D3D11RHI::RSSetFrontCullState()
{
    DeviceContext->RSSetState(FrontCullRasterizerState);
}

void D3D11RHI::RSSetNoCullState()
{
    DeviceContext->RSSetState(NoCullRasterizerState);
}

void D3D11RHI::RSSetDefaultState()
{
    DeviceContext->RSSetState(DefaultRasterizerState);
}

void D3D11RHI::RSSetViewport()
{
    DeviceContext->RSSetViewports(1, &ViewportInfo);

    RefreshFXAAConstantsFromSwapchain();
}


void D3D11RHI::OMSetRenderTargets()
{
    if (FXAARTV)
        DeviceContext->OMSetRenderTargets(1, &FXAARTV, DepthStencilView);
    else 
        DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
}

void D3D11RHI::OMSetSceneRenderTarget()
{
    DeviceContext->OMSetRenderTargets(1, &SceneRenderTargetView, DepthStencilView);
}

void D3D11RHI::OMSetBackBufferOnly()
{
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, nullptr);
}

void D3D11RHI::OMSetBlendState(bool bIsBlendMode)
{
    if (bIsBlendMode == true)
    {
        float blendFactor[4] = { 0,0,0,0 };
        DeviceContext->OMSetBlendState(BlendState, blendFactor, 0xffffffff);
    }
    else
    {
        DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    }
}

void D3D11RHI::Present()
{
    // Draw any Direct2D overlays before present
    UStatsOverlayD2D::Get().Draw();
    SwapChain->Present(1, 0); // vsync on
}

void D3D11RHI::CreateDeviceAndSwapChain(HWND hWindow)
{
    // 지원하는 Direct3D 기능 레벨을 정의
    D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

    // 스왑 체인 설정 구조체 초기화
    DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
    swapchaindesc.BufferDesc.Width = 0; // 창 크기에 맞게 자동으로 설정
    swapchaindesc.BufferDesc.Height = 0; // 창 크기에 맞게 자동으로 설정
    swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 색상 포맷
    swapchaindesc.SampleDesc.Count = 1; // 멀티 샘플링 비활성화
    swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더 타겟으로 사용
    swapchaindesc.BufferCount = 2; // 더블 버퍼링
    swapchaindesc.OutputWindow = hWindow; // 렌더링할 창 핸들
    swapchaindesc.Windowed = TRUE; // 창 모드
    swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 방식

    // Direct3D 장치와 스왑 체인을 생성
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags,
        featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
        &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);
    // 생성된 스왑 체인의 정보 가져오기
    SwapChain->GetDesc(&swapchaindesc);

    // 뷰포트 정보 설정
    ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
}

void D3D11RHI::CreateFrameBuffer()
{
    // 백 버퍼 가져오기
    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

    // 렌더 타겟 뷰 생성
    D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
    framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &RenderTargetView);

    DXGI_SWAP_CHAIN_DESC swapDesc;
    SwapChain->GetDesc(&swapDesc);

    // FXAA SRV생성
    D3D11_TEXTURE2D_DESC FXAAtd = {};
    FXAAtd.Width = swapDesc.BufferDesc.Width;
    FXAAtd.Height = swapDesc.BufferDesc.Height;
    FXAAtd.MipLevels = 1;
    FXAAtd.ArraySize = 1;
    FXAAtd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    FXAAtd.SampleDesc.Count = 1;
    FXAAtd.Usage = D3D11_USAGE_DEFAULT;
    FXAAtd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    Device->CreateTexture2D(&FXAAtd, nullptr, &FXAATex);

    // FXAA용 RTV 생성 
    /*D3D11_RENDER_TARGET_VIEW_DESC FXAARTVdesc = {};
    FXAARTVdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    FXAARTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;*/

    Device->CreateRenderTargetView(FXAATex, nullptr, &FXAARTV);
    Device->CreateShaderResourceView(FXAATex, nullptr, &FXAASRV);
}

void D3D11RHI::CreateDepthBuffer()
{
    DXGI_SWAP_CHAIN_DESC swapDesc;
    SwapChain->GetDesc(&swapDesc);

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = swapDesc.BufferDesc.Width;
    depthDesc.Height = swapDesc.BufferDesc.Height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // SRV를 생성하려면 TYPELESS 포맷 사용
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE; // SRV 바인딩 추가

    ID3D11Texture2D* depthTexture = nullptr;
    Device->CreateTexture2D(&depthDesc, nullptr, &depthTexture);

    // DepthStencilView 생성 (렌더링용)
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DSV는 D24_UNORM_S8_UINT 포맷
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    Device->CreateDepthStencilView(depthTexture, &dsvDesc, &DepthStencilView);

    // ShaderResourceView 생성 (쉐이더에서 깊이값 읽기용)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // SRV는 R24_UNORM_X8_TYPELESS 포맷
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    Device->CreateShaderResourceView(depthTexture, &srvDesc, &DepthShaderResourceView);

    depthTexture->Release(); // 뷰만 참조 유지
}

void D3D11RHI::CreateSceneRenderTarget()
{
    // BackBuffer와 동일한 크기/포맷으로 Scene Texture 생성
    D3D11_TEXTURE2D_DESC backBufferDesc;
    FrameBuffer->GetDesc(&backBufferDesc);

    D3D11_TEXTURE2D_DESC sceneTextureDesc = {};
    sceneTextureDesc.Width = backBufferDesc.Width;
    sceneTextureDesc.Height = backBufferDesc.Height;
    sceneTextureDesc.MipLevels = 1;
    sceneTextureDesc.ArraySize = 1;
    sceneTextureDesc.Format = backBufferDesc.Format; // 동일한 포맷 사용
    sceneTextureDesc.SampleDesc.Count = 1;
    sceneTextureDesc.SampleDesc.Quality = 0;
    sceneTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    sceneTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    sceneTextureDesc.CPUAccessFlags = 0;
    sceneTextureDesc.MiscFlags = 0;

	ID3D11Texture2D* SceneTexture = nullptr;
    HRESULT hr = Device->CreateTexture2D(&sceneTextureDesc, nullptr, &SceneTexture);
    if (FAILED(hr))
    {
        UE_LOG("ERROR: Failed to create Scene Texture");
        return;
    }

    // Scene RenderTargetView 생성
    hr = Device->CreateRenderTargetView(SceneTexture, nullptr, &SceneRenderTargetView);
    if (FAILED(hr))
    {
        UE_LOG("ERROR: Failed to create Scene RenderTargetView");
        return;
    }

    // Scene ShaderResourceView 생성
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = sceneTextureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = Device->CreateShaderResourceView(SceneTexture, &srvDesc, &SceneShaderResourceView);
    if (FAILED(hr))
    {
        UE_LOG("ERROR: Failed to create Scene ShaderResourceView");
        return;
    }

    UE_LOG("SUCCESS: Scene RenderTarget created (%ux%u)", sceneTextureDesc.Width, sceneTextureDesc.Height);

	SceneTexture->Release(); // 뷰만 참조 유지
}

void D3D11RHI::CreateRasterizerState()
{
    // 이미 생성된 경우 중복 생성 방지
    if (DefaultRasterizerState)
        return;

    D3D11_RASTERIZER_DESC deafultrasterizerdesc = {};
    deafultrasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
    deafultrasterizerdesc.CullMode = D3D11_CULL_BACK; // 백 페이스 컬링
    deafultrasterizerdesc.DepthClipEnable = TRUE; // 근/원거리 평면 클리핑

    Device->CreateRasterizerState(&deafultrasterizerdesc, &DefaultRasterizerState);

    D3D11_RASTERIZER_DESC wireframerasterizerdesc = {};
    wireframerasterizerdesc.FillMode = D3D11_FILL_WIREFRAME; // 채우기 모드
    wireframerasterizerdesc.CullMode = D3D11_CULL_BACK; // 백 페이스 컬링
    wireframerasterizerdesc.DepthClipEnable = TRUE; // 근/원거리 평면 클리핑

    Device->CreateRasterizerState(&wireframerasterizerdesc, &WireFrameRasterizerState);

    D3D11_RASTERIZER_DESC frontcullrasterizerdesc = {};
    frontcullrasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
    frontcullrasterizerdesc.CullMode = D3D11_CULL_FRONT; // 프론트 페이스 컬링
    frontcullrasterizerdesc.DepthClipEnable = TRUE; // 근/원거리 평면 클리핑

    Device->CreateRasterizerState(&frontcullrasterizerdesc, &FrontCullRasterizerState);

    D3D11_RASTERIZER_DESC nocullrasterizerdesc = {};
    nocullrasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
    nocullrasterizerdesc.CullMode = D3D11_CULL_NONE; // 컬링 없음
    nocullrasterizerdesc.DepthClipEnable = TRUE; // 근/원거리 평면 클리핑

    Device->CreateRasterizerState(&nocullrasterizerdesc, &NoCullRasterizerState);
}

void D3D11RHI::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC modelDesc = {};
    modelDesc.Usage = D3D11_USAGE_DYNAMIC;
    modelDesc.ByteWidth = sizeof(ModelBufferType);
    modelDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    modelDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&modelDesc, nullptr, &ModelCB);

    // b0 in StaticMeshPS
    D3D11_BUFFER_DESC pixelConstDesc = {};
    pixelConstDesc.Usage = D3D11_USAGE_DYNAMIC;
    pixelConstDesc.ByteWidth = sizeof(FPixelConstBufferType);
    pixelConstDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    pixelConstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = Device->CreateBuffer(&pixelConstDesc, nullptr, &PixelConstCB);
    if (FAILED(hr))
    {
        assert(FAILED(hr));
    }

    // b1 : ViewProjBuffer
    D3D11_BUFFER_DESC vpDesc = {};
    vpDesc.Usage = D3D11_USAGE_DYNAMIC;
    vpDesc.ByteWidth = sizeof(ViewProjBufferType);
    vpDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&vpDesc, nullptr, &ViewProjCB);

    // b2 : HighLightBuffer  (← 기존 코드에서 vpDesc를 다시 써서 버그났던 부분)
    D3D11_BUFFER_DESC hlDesc = {};
    hlDesc.Usage = D3D11_USAGE_DYNAMIC;
    hlDesc.ByteWidth = sizeof(HighLightBufferType);
    hlDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hlDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&hlDesc, nullptr, &HighLightCB);

    D3D11_BUFFER_DESC billboardDesc = {};
    billboardDesc.Usage = D3D11_USAGE_DYNAMIC;
    billboardDesc.ByteWidth = sizeof(BillboardBufferType);
    billboardDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    billboardDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&billboardDesc, nullptr, &BillboardCB);

    D3D11_BUFFER_DESC ColorDesc = {};
    ColorDesc.Usage = D3D11_USAGE_DYNAMIC;
    ColorDesc.ByteWidth = sizeof(ColorBufferType);
    ColorDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ColorDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&ColorDesc, nullptr, &ColorCB);

    D3D11_BUFFER_DESC uvScrollDesc = {};
    uvScrollDesc.Usage = D3D11_USAGE_DYNAMIC;
    uvScrollDesc.ByteWidth = sizeof(float) * 4; // float2 speed + float time + pad
    uvScrollDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    uvScrollDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&uvScrollDesc, nullptr, &UVScrollCB);
    if (UVScrollCB)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(DeviceContext->Map(UVScrollCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            float init[4] = { 0,0,0,0 };
            memcpy(mapped.pData, init, sizeof(init));
            DeviceContext->Unmap(UVScrollCB, 0);
        }
        DeviceContext->PSSetConstantBuffers(5, 1, &UVScrollCB);
    }

    // b4 : InvWorldBuffer (데칼용 - InvWorld + InvViewProj)
    D3D11_BUFFER_DESC invWorldDesc = {};
    invWorldDesc.Usage = D3D11_USAGE_DYNAMIC;
    invWorldDesc.ByteWidth = sizeof(FMatrix) * 2; // InvWorldMatrix + InvViewProjMatrix
    invWorldDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    invWorldDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&invWorldDesc, nullptr, &InvWorldCB);

    // b5 : ViewportBuffer (데칼용 - Viewport 정보)
    D3D11_BUFFER_DESC viewportDesc = {};
    viewportDesc.Usage = D3D11_USAGE_DYNAMIC;
    viewportDesc.ByteWidth = sizeof(ViewportBufferType);
    viewportDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    viewportDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&viewportDesc, nullptr, &ViewportCB);

    // b7 : Light array (FLightGPU[MAX_LIGHTS])
    D3D11_BUFFER_DESC lightDesc = {};
    lightDesc.Usage = D3D11_USAGE_DYNAMIC;
    lightDesc.ByteWidth = sizeof(FLightInfo) * MAX_LIGHT_COUNT;
    lightDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    lightDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&lightDesc, nullptr, &LightCB);  

    //b0 : FXAA 
    D3D11_BUFFER_DESC fxaaDesc = {};
    fxaaDesc.Usage = D3D11_USAGE_DYNAMIC;
    fxaaDesc.ByteWidth = sizeof(FXAAInfo);
    fxaaDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    fxaaDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&fxaaDesc, nullptr, &FXAACB);


    // b6 : DepthVisualizationBuffer (Scene Depth 시각화용)
    D3D11_BUFFER_DESC depthVisDesc = {};
    depthVisDesc.Usage = D3D11_USAGE_DYNAMIC;
    depthVisDesc.ByteWidth = sizeof(float) * 8; // 8개의 float (32 bytes)
    depthVisDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    depthVisDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&depthVisDesc, nullptr, &DepthVisualizationCB);

    // b0: Camera Buffer (Fog용 - Near/Far Plane)
    D3D11_BUFFER_DESC cameraFogDesc = {};
    cameraFogDesc.Usage = D3D11_USAGE_DYNAMIC;
    cameraFogDesc.ByteWidth = sizeof(float) * 4; // NearPlane, FarPlane, Padding[2]
    cameraFogDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cameraFogDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&cameraFogDesc, nullptr, &CameraNearFarCB);

    // b1: Fog Parameter Buffer
    D3D11_BUFFER_DESC fogParamDesc = {};
    fogParamDesc.Usage = D3D11_USAGE_DYNAMIC;
    fogParamDesc.ByteWidth = sizeof(float) * 16; // 64 bytes (4 float4)
    fogParamDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    fogParamDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&fogParamDesc, nullptr, &FogParameterCB);

    // b2: Inverse Matrix Buffer
    D3D11_BUFFER_DESC invMatrixDesc = {};
    invMatrixDesc.Usage = D3D11_USAGE_DYNAMIC;
    invMatrixDesc.ByteWidth = sizeof(FMatrix) * 2; // InvView + InvProj
    invMatrixDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    invMatrixDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&invMatrixDesc, nullptr, &InvViewProjCB);

    // b3: Viewport Buffer (Fog용)
    D3D11_BUFFER_DESC viewportFogDesc = {};
    viewportFogDesc.Usage = D3D11_USAGE_DYNAMIC;
    viewportFogDesc.ByteWidth = sizeof(float) * 8; // 32 bytes (2 float4)
    viewportFogDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    viewportFogDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&viewportFogDesc, nullptr, &ViewportFogCB);

    // b7: CopyShader Viewport Buffer
    D3D11_BUFFER_DESC copyViewportDesc = {};
    copyViewportDesc.Usage = D3D11_USAGE_DYNAMIC;
    copyViewportDesc.ByteWidth = sizeof(float) * 8; // 32 bytes (2 float4)
    copyViewportDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    copyViewportDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&copyViewportDesc, nullptr, &CopyShaderViewportCB);
}

void D3D11RHI::UpdateUVScrollConstantBuffers(const FVector2D& Speed, float TimeSec)
{
    if (!UVScrollCB) return;

    struct { float x; float y; float t; float pad; } data { Speed.X, Speed.Y, TimeSec, 0.0f };

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(UVScrollCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(data));
        DeviceContext->Unmap(UVScrollCB, 0);
        DeviceContext->PSSetConstantBuffers(5, 1, &UVScrollCB);
    }
}

void D3D11RHI::UpdateInvWorldConstantBuffer(const FMatrix& InvWorldMatrix, const FMatrix& InvViewProjMatrix)
{
    if (!InvWorldCB) return;

    struct InvWorldBufferType
    {
        FMatrix InvWorld;
        FMatrix InvViewProj;
    };

    InvWorldBufferType data;
    data.InvWorld = InvWorldMatrix;
    data.InvViewProj = InvViewProjMatrix;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(InvWorldCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(InvWorldBufferType));
        DeviceContext->Unmap(InvWorldCB, 0);
        DeviceContext->PSSetConstantBuffers(4, 1, &InvWorldCB);
    }
}

void D3D11RHI::UpdateViewportConstantBuffer(float StartX, float StartY, float SizeX, float SizeY)
{
    if (!ViewportCB) return;

    ViewportBufferType data;
    data.ViewportRect = FVector4(StartX, StartY, SizeX, SizeY);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(ViewportCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(ViewportBufferType));
        DeviceContext->Unmap(ViewportCB, 0);
        DeviceContext->PSSetConstantBuffers(6, 1, &ViewportCB);
    }
}

void D3D11RHI::UpdateViewportConstantBuffer(float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight, float ScreenWidth, float ScreenHeight)
{
    if (!ViewportFogCB) return;

    struct ViewportBufferType {
        float ViewportPos[2];
        float ViewportSize[2];
        float ScreenSize[2];
        float Padding[2];
    };

    ViewportBufferType data;
    data.ViewportPos[0] = ViewportX;
    data.ViewportPos[1] = ViewportY;
    data.ViewportSize[0] = ViewportWidth;
    data.ViewportSize[1] = ViewportHeight;
    data.ScreenSize[0] = ScreenWidth;
    data.ScreenSize[1] = ScreenHeight;
    data.Padding[0] = data.Padding[1] = 0.0f;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(ViewportFogCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(ViewportBufferType));
        DeviceContext->Unmap(ViewportFogCB, 0);
        DeviceContext->PSSetConstantBuffers(3, 1, &ViewportFogCB);
    }
}

void D3D11RHI::UpdateDepthVisualizationBuffer(float NearPlane, float FarPlane, float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight, float ScreenWidth, float ScreenHeight)
{
    if (!DepthVisualizationCB) return;

    struct DepthVisualizationBufferType
    {
        float NearPlane;
        float FarPlane;
        float ViewportPosX;
        float ViewportPosY;
        float ViewportSizeX;
        float ViewportSizeY;
        float ScreenSizeX;
        float ScreenSizeY;
    };

    DepthVisualizationBufferType data;
    data.NearPlane = NearPlane;
    data.FarPlane = FarPlane;
    data.ViewportPosX = ViewportX;
    data.ViewportPosY = ViewportY;
    data.ViewportSizeX = ViewportWidth;
    data.ViewportSizeY = ViewportHeight;
    data.ScreenSizeX = ScreenWidth;
    data.ScreenSizeY = ScreenHeight;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(DepthVisualizationCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(DepthVisualizationBufferType));
        DeviceContext->Unmap(DepthVisualizationCB, 0);
        DeviceContext->PSSetConstantBuffers(6, 1, &DepthVisualizationCB);
    }
}

void D3D11RHI::UpdateCameraNearFarConstantBuffer(float NearPlane, float FarPlane)
{
    if (!CameraNearFarCB) return;

    struct CameraBufferType {
        float NearPlane;
        float FarPlane;
        float Padding[2];
    };

    CameraBufferType data;
    data.NearPlane = NearPlane;
    data.FarPlane = FarPlane;
    data.Padding[0] = 0.0f;
    data.Padding[1] = 0.0f;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(CameraNearFarCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(CameraBufferType));
        DeviceContext->Unmap(CameraNearFarCB, 0);
        DeviceContext->PSSetConstantBuffers(0, 1, &CameraNearFarCB);
    }
}

void D3D11RHI::UpdateFogParameterConstantBuffer(float FogDensity, float FogHeightFalloff, float FogStartDistance, float FogCutoffDistance, float FogMaxOpacity, const FVector4& FogInscatteringColor, const FVector& FogComponentPosition)
{
    if (!FogParameterCB) return;

    struct FogParameterBufferType {
        float FogDensity;
        float FogHeightFalloff;
        float FogStartDistance;
        float FogCutoffDistance;

        float FogMaxOpacity;
        float Padding1[3];

        FVector4 FogInscatteringColor;

        FVector FogComponentPosition;
        float Padding2;
    };

    FogParameterBufferType data;
    data.FogDensity = FogDensity;
    data.FogHeightFalloff = FogHeightFalloff;
    data.FogStartDistance = FogStartDistance;
    data.FogCutoffDistance = FogCutoffDistance;
    data.FogMaxOpacity = FogMaxOpacity;
    data.Padding1[0] = data.Padding1[1] = data.Padding1[2] = 0.0f;
    data.FogInscatteringColor = FogInscatteringColor;
    data.FogComponentPosition = FogComponentPosition;
    data.Padding2 = 0.0f;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(FogParameterCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(FogParameterBufferType));
        DeviceContext->Unmap(FogParameterCB, 0);
        DeviceContext->PSSetConstantBuffers(1, 1, &FogParameterCB);
    }
}

void D3D11RHI::UpdateInverseViewProjMatrixConstantBuffer(const FMatrix& InvViewMatrix, const FMatrix& InvProjectionMatrix)
{
    if (!InvViewProjCB) return;

    struct InverseMatrixBufferType {
        FMatrix InvView;
        FMatrix InvProjection;
    };

    InverseMatrixBufferType data;
    data.InvView = InvViewMatrix;
    data.InvProjection = InvProjectionMatrix;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(InvViewProjCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(InverseMatrixBufferType));
        DeviceContext->Unmap(InvViewProjCB, 0);
        DeviceContext->PSSetConstantBuffers(2, 1, &InvViewProjCB);
    }
}

void D3D11RHI::UpdateCopyShaderViewportBuffer(float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight, float ScreenWidth, float ScreenHeight)
{
    if (!CopyShaderViewportCB) return;

    struct CopyViewportBufferType {
        float ViewportPos[2];    // g_ViewportPos
        float ViewportSize[2];   // g_ViewportSize
        float ScreenSize[2];     // g_ScreenSize
        float Padding[2];        // 16바이트 정렬
    };

    CopyViewportBufferType data;
    data.ViewportPos[0] = ViewportX;
    data.ViewportPos[1] = ViewportY;
    data.ViewportSize[0] = ViewportWidth;
    data.ViewportSize[1] = ViewportHeight;
    data.ScreenSize[0] = ScreenWidth;
    data.ScreenSize[1] = ScreenHeight;
    data.Padding[0] = 0.0f;
    data.Padding[1] = 0.0f;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(DeviceContext->Map(CopyShaderViewportCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &data, sizeof(CopyViewportBufferType));
        DeviceContext->Unmap(CopyShaderViewportCB, 0);
        DeviceContext->PSSetConstantBuffers(7, 1, &CopyShaderViewportCB); // b7 슬롯
    }
}

void D3D11RHI::ReleaseSamplerState()
{
    if (DefaultSamplerState)
    {
        DefaultSamplerState->Release();
        DefaultSamplerState = nullptr;
	}
}

void D3D11RHI::ReleaseBlendState()
{
    if (BlendState)
    {
        BlendState->Release();
        BlendState = nullptr;
    }
}

void D3D11RHI::ReleaseRasterizerState()
{
    if (DefaultRasterizerState)
    {
        DefaultRasterizerState->Release();
        DefaultRasterizerState = nullptr;
    }
    if (WireFrameRasterizerState)
    {
        WireFrameRasterizerState->Release();
        WireFrameRasterizerState = nullptr;
    }
    if (FrontCullRasterizerState)
    {
        FrontCullRasterizerState->Release();
        FrontCullRasterizerState = nullptr;
    }
    if (NoCullRasterizerState)
    {
        NoCullRasterizerState->Release();
        NoCullRasterizerState = nullptr;
    }
    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void D3D11RHI::ReleaseFrameBuffer()
{
    if (FrameBuffer)
    {
        FrameBuffer->Release();
        FrameBuffer = nullptr;
    }
    if (FXAATex)
    {
        FXAATex->Release();
        FXAATex = nullptr;
    }
    if (RenderTargetView)
    {
        RenderTargetView->Release();
        RenderTargetView = nullptr;
    }
    if (FXAARTV)
    {
        FXAARTV->Release();
    }
    if (FXAASRV)
    {
        FXAASRV->Release();
        FXAASRV = nullptr;
    }
}

void D3D11RHI::ReleaseDepthBuffer()
{
    if (DepthStencilView)
    {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
    }

    if (DepthShaderResourceView)
    {
        DepthShaderResourceView->Release();
        DepthShaderResourceView = nullptr;
	}
}

void D3D11RHI::ReleaseSceneRenderTarget()
{
    if (SceneRenderTargetView)
    {
        SceneRenderTargetView->Release();
        SceneRenderTargetView = nullptr;
    }

    if (SceneShaderResourceView)
    {
        SceneShaderResourceView->Release();
        SceneShaderResourceView = nullptr;
	}
}

void D3D11RHI::ReleaseDeviceAndSwapChain()
{
    if (SwapChain)
    {
        SwapChain->Release();
        SwapChain = nullptr;
    }

    if (DeviceContext)
    {
        DeviceContext->Release();
        DeviceContext = nullptr;
    }

    if (Device)
    {
        Device->Release();
        Device = nullptr;
    }

}

void D3D11RHI::OmSetDepthStencilState(EComparisonFunc Func)
{
    switch (Func)
    {
    case EComparisonFunc::Always:
        DeviceContext->OMSetDepthStencilState(DepthStencilStateAlwaysNoWrite, 0);
        break;
    case EComparisonFunc::LessEqual:
        DeviceContext->OMSetDepthStencilState(DepthStencilStateLessEqualWrite, 0);
        break;
    case EComparisonFunc::LessEqualReadOnly:
        DeviceContext->OMSetDepthStencilState(DepthStencilStateLessEqualReadOnly, 0);
        break;
    case EComparisonFunc::GreaterEqual:
        DeviceContext->OMSetDepthStencilState(DepthStencilStateGreaterEqualWrite, 0);
        break;
    }
}

void D3D11RHI::CreateShader(ID3D11InputLayout** SimpleInputLayout, ID3D11VertexShader** SimpleVertexShader, ID3D11PixelShader** SimplePixelShader)
{
    ID3DBlob* vertexshaderCSO;
    ID3DBlob* pixelshaderCSO;

    D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

    Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, SimpleVertexShader);

    D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

    Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, SimplePixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), SimpleInputLayout);

    vertexshaderCSO->Release();
    pixelshaderCSO->Release();
}
void D3D11RHI::OnResize(UINT NewWidth, UINT NewHeight)
{
    if (!Device || !DeviceContext || !SwapChain)
        return;

    // 기존 리소스 해제
    ReleaseFrameBuffer();

    // 스왑체인 버퍼 리사이즈
    HRESULT hr = SwapChain->ResizeBuffers(
        0,                 // 버퍼 개수 (0 = 기존 유지)
        NewWidth,
        NewHeight,
        DXGI_FORMAT_UNKNOWN, // 기존 포맷 유지
        0
    );
    if (FAILED(hr))
    {
        UE_LOG("SwapChain->ResizeBuffers failed!\n");
        return;
    }

    // 새 프레임버퍼/RTV/DSV 생성
    CreateFrameBuffer();

    // 뷰포트 갱신
    ViewportInfo.TopLeftX = 0.0f;
    ViewportInfo.TopLeftY = 0.0f;
    ViewportInfo.Width = static_cast<float>(NewWidth);
    ViewportInfo.Height = static_cast<float>(NewHeight);
    ViewportInfo.MinDepth = 0.0f;
    ViewportInfo.MaxDepth = 1.0f;

    DeviceContext->RSSetViewports(1, &ViewportInfo);

    // Update FXAA constants to match new resolution
    RefreshFXAAConstantsFromSwapchain();

}

//void D3D11RHI::CreateBackBufferAndDepthStencil(UINT width, UINT height)
//{
//    // 기존 바인딩 해제 후 뷰 해제
//    if (RenderTargetView) { DeviceContext->OMSetRenderTargets(0, nullptr, nullptr); RenderTargetView->Release(); RenderTargetView = nullptr; }
//    if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }
//    \
//    // 1) 백버퍼에서 RTV 생성
//    ID3D11Texture2D* backBuffer = nullptr;
//    HRESULT hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
//    if (FAILED(hr) || !backBuffer) {
//        UE_LOG("GetBuffer(0) failed.\n");
//        return;
//    }

// ──────────────────────────────────────────────────────
// Helper: Viewport 갱신
// ──────────────────────────────────────────────────────
void D3D11RHI::SetViewport(UINT width, UINT height)
{
    ViewportInfo.TopLeftX = 0.0f;
    ViewportInfo.TopLeftY = 0.0f;
    ViewportInfo.Width = static_cast<float>(width);
    ViewportInfo.Height = static_cast<float>(height);
    ViewportInfo.MinDepth = 0.0f;
    ViewportInfo.MaxDepth = 1.0f;

    DeviceContext->RSSetViewports(1, &ViewportInfo);
}

// ──────────────────────────────────────────────────────
// 기존 오타 호출 호환용 래퍼 (선택)
// ──────────────────────────────────────────────────────
void D3D11RHI::setviewort(UINT width, UINT height)
{
    SetViewport(width, height);
}

void D3D11RHI::ResizeSwapChain(UINT width, UINT height)
{
    if (!SwapChain) return;

    // ============================================================
    // 1. GPU 작업 완료 대기 및 파이프라인 언바인딩
    // ============================================================
    if (DeviceContext)
    {
        DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        DeviceContext->Flush();
    }

    // ============================================================
    // 2. 기존 리소스 해제 (역순)
    // ============================================================
    ReleaseSceneRenderTarget(); // Scene RenderTarget 먼저 해제
    ReleaseDepthBuffer();        // Depth Buffer 해제
    ReleaseFrameBuffer();        // BackBuffer 해제 (마지막)

    // ============================================================
    // 3. SwapChain 크기 조정
    // ============================================================
    HRESULT hr = SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        UE_LOG("ERROR: SwapChain->ResizeBuffers failed! (HRESULT: 0x%X)", hr);
        return;
    }

    // ============================================================
    // 4. 새 리소스 생성 (정순)
    // ============================================================
    CreateFrameBuffer();         // BackBuffer RTV 생성
    CreateDepthBuffer();         // Depth Buffer + SRV 생성 (새 크기)
    CreateSceneRenderTarget();   // Scene RenderTarget + SRV 생성 (새 크기)

    // ============================================================
    // 5. Viewport 갱신
    // ============================================================
    SetViewport(width, height);

    // ============================================================
    // 6. 초기 상태 설정
    // ============================================================
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);

    UE_LOG("SUCCESS: ResizeSwapChain completed (%ux%u)", width, height);
}

void D3D11RHI::PSSetDefaultSampler(UINT StartSlot)
{
    DeviceContext->PSSetSamplers(StartSlot, 1, &DefaultSamplerState);
}

void D3D11RHI::OMSetBackBufferNoDepth()
{
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, nullptr);
}

void D3D11RHI::UpdateViewportCBFromCurrent()
{
    UpdateViewportConstantBuffer(ViewportInfo.TopLeftX, ViewportInfo.TopLeftY, ViewportInfo.Width, ViewportInfo.Height);
}

void D3D11RHI::RefreshFXAAConstantsFromSwapchain()
{  
    float w = 0.0f, h = 0.0f;
    if (SwapChain)
    {
        DXGI_SWAP_CHAIN_DESC scd{};
        SwapChain->GetDesc(&scd);
        w = static_cast<float>(scd.BufferDesc.Width);
        h = static_cast<float>(scd.BufferDesc.Height);
    }
    if (w <= 0.0f || h <= 0.0f)
        return;

    FXAAInfo info{};
    info.InvResolution[0] = 1.0f / static_cast<float>(ViewportInfo.Width);
    info.InvResolution[1] = 1.0f / static_cast<float>(ViewportInfo.Height);

    info.Enabled = bFXAAEnabledFlag;

    // Only set params if enabled
    if (bFXAAEnabledFlag)
    {
        // Default values from NVIDIA FXAA 3.11
        info.FXAASpanMax = 8.0f;
        info.FXAAReduceMul = 1.0f / 8.0f;
        info.FXAAReduceMin = 1.0f / 128.0f;
    }

    UpdateFXAAConstantBuffers(info);
}

void D3D11RHI::UpdateLightConstantBuffers(const TArray<FLightInfo>& InLights)
{
    if (!LightCB) return;
    D3D11_MAPPED_SUBRESOURCE mapped{};

    if (SUCCEEDED(DeviceContext->Map(LightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        const size_t maxBytes = sizeof(FLightInfo) * static_cast<size_t>(MAX_LIGHT_COUNT);
        memset(mapped.pData, 0, maxBytes);
         
        size_t copyCount = InLights.size();

        if (copyCount > static_cast<size_t>(MAX_LIGHT_COUNT))
            copyCount = static_cast<size_t>(MAX_LIGHT_COUNT);

        const size_t bytes = sizeof(FLightInfo) * copyCount;
        if (bytes > 0)
        {
            memcpy(mapped.pData, InLights.data(), bytes);
        }

        DeviceContext->Unmap(LightCB, 0);
        DeviceContext->PSSetConstantBuffers(7, 1, &LightCB);
    } 
}

void D3D11RHI::UpdateFXAAConstantBuffers(const FXAAInfo& InFXAAInfo)
{
    if (!FXAACB) return;
    D3D11_MAPPED_SUBRESOURCE mapped{};
     
    if (SUCCEEDED(DeviceContext->Map(FXAACB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {  
        auto* dataPtr = reinterpret_cast<FXAAInfo*>(mapped.pData);
        *dataPtr = InFXAAInfo;
        DeviceContext->Unmap(FXAACB, 0);
        // Bind to PS b0
        DeviceContext->PSSetConstantBuffers(0, 1, &FXAACB);
    }
}
