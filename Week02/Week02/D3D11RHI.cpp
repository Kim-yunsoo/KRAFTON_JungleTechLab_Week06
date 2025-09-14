#include "D3D11RHI.h"
#include "Vector.h"
#include "UI/GlobalConsole.h"

struct FConstants
{
    FVector WorldPosition;
    float Scale;
};
// b0
struct ModelBufferType
{
    FMatrix Model;
};

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

struct BillboardBufferType
{
    FMatrix ViewProj;
    FVector cameraRight;
    FVector cameraUp;
};

void D3D11RHI::Initialize(HWND hWindow)
{
    // 이곳에서 Device, DeviceContext, viewport, swapchain를 초기화한다
    CreateDeviceAndSwapChain(hWindow);
    CreateFrameBuffer();
    CreateRasterizerState();
    CreateConstantBuffer();
    UResourceManager::GetInstance().Initialize(Device,DeviceContext);
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

    // 상수버퍼
    if (HighLightCB) { HighLightCB->Release(); HighLightCB = nullptr; }
    if (ModelCB) { ModelCB->Release(); ModelCB = nullptr; }
    if (ViewProjCB) { ViewProjCB->Release(); ViewProjCB = nullptr; }
    if (ConstantBuffer) { ConstantBuffer->Release(); ConstantBuffer = nullptr; }

    // 상태 객체
    if (DepthStencilState) { DepthStencilState->Release(); DepthStencilState = nullptr; }
    if (RasterizerState) { RasterizerState->Release();   RasterizerState = nullptr; }
    if (BlendState) { BlendState->Release();        BlendState = nullptr; }

    // RTV/DSV/FrameBuffer
    ReleaseFrameBuffer();

    // Device + SwapChain
    ReleaseDeviceAndSwapChain();
}

void D3D11RHI::ClearBackBuffer()
{
    float ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
    DeviceContext->ClearRenderTargetView(RenderTargetView, ClearColor);
}

void D3D11RHI::ClearDepthBuffer(float Depth, UINT Stencil)
{
    DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, Depth, Stencil);

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
    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Device->CreateBlendState(&bd, &BlendState);
}

//이거 두개를 나눔
void D3D11RHI::UpdateConstantBuffers(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
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

    // b1 : 뷰/프로젝션 행렬
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        DeviceContext->Map(ViewProjCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* dataPtr = reinterpret_cast<ViewProjBufferType*>(mapped.pData);

        dataPtr->View = ViewMatrix;
        dataPtr->Proj = ProjMatrix;

        DeviceContext->Unmap(ViewProjCB, 0);
        DeviceContext->VSSetConstantBuffers(1, 1, &ViewProjCB); // b1 슬롯
    }
}

void D3D11RHI::UpdateBillboardConstantBuffers(const FMatrix& ViewMatrix, const FMatrix& ProjMatrix,
    const FVector& CameraRight, const FVector& CameraUp)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    DeviceContext->Map(BillboardCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    auto* dataPtr = reinterpret_cast<BillboardBufferType*>(mapped.pData);

    // HLSL 기본 row-major와 맞추기 위해 전치
    dataPtr->ViewProj = ViewMatrix*ProjMatrix;
    dataPtr->cameraRight = CameraRight;
    dataPtr->cameraUp = CameraUp;

    DeviceContext->Unmap(BillboardCB, 0);
    DeviceContext->VSSetConstantBuffers(0, 1, &BillboardCB); // b0 슬롯
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
       }
}

void D3D11RHI::IASetPrimitiveTopology()
{
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D3D11RHI::RSSetViewport()
{
    DeviceContext->RSSetViewports(1, &ViewportInfo);
}

void D3D11RHI::RSSetState()
{
    DeviceContext->RSSetState(RasterizerState);
}

void D3D11RHI::OMSetRenderTargets()
{
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
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
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
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

    // =====================================
    // 깊이/스텐실 버퍼 생성
    // =====================================
    DXGI_SWAP_CHAIN_DESC swapDesc;
    SwapChain->GetDesc(&swapDesc);

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = swapDesc.BufferDesc.Width;
    depthDesc.Height = swapDesc.BufferDesc.Height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 깊이 24비트 + 스텐실 8비트
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* depthBuffer = nullptr;
    Device->CreateTexture2D(&depthDesc, nullptr, &depthBuffer);

    // DepthStencilView 생성
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = depthDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    Device->CreateDepthStencilView(depthBuffer, &dsvDesc, &DepthStencilView);

    depthBuffer->Release(); // 뷰만 참조 유지
}

void D3D11RHI::CreateRasterizerState()
{
    D3D11_RASTERIZER_DESC rasterizerdesc = {};
    rasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
    rasterizerdesc.CullMode = D3D11_CULL_BACK; // 백 페이스 컬링

    Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
}

void D3D11RHI::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC modelDesc = {};
    modelDesc.Usage = D3D11_USAGE_DYNAMIC;
    modelDesc.ByteWidth = sizeof(ModelBufferType);
    modelDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    modelDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&modelDesc, nullptr, &ModelCB);

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
    billboardDesc.ByteWidth = sizeof(HighLightBufferType);
    billboardDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    billboardDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Device->CreateBuffer(&billboardDesc, nullptr, &BillboardCB);
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
    if (RasterizerState)
    {
        RasterizerState->Release();
        RasterizerState = nullptr;
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
    if (RenderTargetView)
    {
        RenderTargetView->Release();
        RenderTargetView = nullptr;
    }

    if (DepthStencilView)
    {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
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
}
void D3D11RHI::CreateBackBufferAndDepthStencil(UINT width, UINT height)
{
    // 기존 바인딩 해제 후 뷰 해제
    if (RenderTargetView) { DeviceContext->OMSetRenderTargets(0, nullptr, nullptr); RenderTargetView->Release(); RenderTargetView = nullptr; }
    if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }

    // 1) 백버퍼에서 RTV 생성
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr) || !backBuffer) {
        UE_LOG("GetBuffer(0) failed.\n");
        return;
    }

    // 백버퍼 포맷은 스왑체인과 동일. 특별한 이유 없으면 RTV desc는 nullptr로 두는 것이 안전.
    hr = Device->CreateRenderTargetView(backBuffer, nullptr, &RenderTargetView);
    backBuffer->Release();
    if (FAILED(hr) || !RenderTargetView) {
        UE_LOG("CreateRenderTargetView failed.\n");
        return;
    }

    // 2) DepthStencil 텍스처/뷰 생성
    ID3D11Texture2D* depthTex = nullptr;
    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;               // 멀티샘플링 끄는 경우
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = Device->CreateTexture2D(&depthDesc, nullptr, &depthTex);
    if (FAILED(hr) || !depthTex) {
        UE_LOG("CreateTexture2D(depth) failed.\n");
        return;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = depthDesc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateDepthStencilView(depthTex, &dsvDesc, &DepthStencilView);
    depthTex->Release();
    if (FAILED(hr) || !DepthStencilView) {
        UE_LOG("CreateDepthStencilView failed.\n");
        return;
    }

    // 3) OM 바인딩
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);

    // 4) 뷰포트 갱신
    SetViewport(width, height);
}

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

    // 기존 뷰 해제
    if (RenderTargetView) { RenderTargetView->Release(); RenderTargetView = nullptr; }
    if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }
    if (FrameBuffer) { FrameBuffer->Release(); FrameBuffer = nullptr; }

    // 스왑체인 버퍼 리사이즈
    HRESULT hr = SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) { UE_LOG("ResizeBuffers failed!\n"); return; }

    // 다시 RTV/DSV 만들기
    CreateBackBufferAndDepthStencil(width, height);

    // 뷰포트도 갱신
    setviewort(width, height);
}
