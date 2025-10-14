#pragma once
#include "RHIDevice.h"
#include "ResourceManager.h"

class D3D11RHI : public URHIDevice
{
public:
    D3D11RHI() {};
    ~D3D11RHI()
    {
        Release();
    }



public:
    void Initialize(HWND hWindow) override;

    void Release()override;


public:
    // clear
    void ClearBackBuffer() override;
    void ClearDepthBuffer(float Depth, UINT Stencil) override;
    void CreateBlendState() override;

    template<typename TVertex>
    static HRESULT CreateVertexBufferImpl(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer, D3D11_USAGE usage, UINT cpuAccessFlags);

    template<typename TVeretex>
    static HRESULT CreateVertexBuffer(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer);

    template<typename TVertex>
    static HRESULT CreateVertexBufferImpl(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer, D3D11_USAGE usage, UINT cpuAccessFlags);

    template<typename TVertex>
    static HRESULT CreateVertexBuffer(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer);

    static HRESULT CreateIndexBuffer(ID3D11Device* device, const FMeshData* meshData, ID3D11Buffer** outBuffer);

    static HRESULT CreateIndexBuffer(ID3D11Device* device, const FStaticMesh* mesh, ID3D11Buffer** outBuffer);
     
    void UpdateConstantBuffers(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix) override;
    void UpdateViewConstantBuffers( const FMatrix& ViewMatrix, const FMatrix& ProjMatrix) ;
    void UpdateModelConstantBuffers(const FMatrix& ModelMatrix) ;
    void UpdateBillboardConstantBuffers(const FVector& pos, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, const FVector& CameraRight, const FVector& CameraUp) override;
    void UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture) override;
    void UpdateHighLightConstantBuffers(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo) override;
    void UpdateColorConstantBuffers(const FVector4& InColor) override;
    void UpdateUVScrollConstantBuffers(const FVector2D& Speed, float TimeSec) override;
    void UpdateInvWorldConstantBuffer(const FMatrix& InvWorldMatrix, const FMatrix& InvViewProjMatrix) override;
    void UpdateViewportConstantBuffer(float StartX, float StartY, float SizeX, float SizeY);
    void UpdateLightConstantBuffers(const TArray<FLightInfo>& InLights) override;
    void UpdateFXAAConstantBuffers(const FXAAInfo& InFXAAInfo) override;

    void IASetPrimitiveTopology() override;
    void RSSetState(EViewModeIndex ViewModeIndex) override;
    void RSSetFrontCullState() override;
    void RSSetNoCullState() override;
    void RSSetDefaultState() override;
    void RSSetViewport() override;
    void OMSetRenderTargets() override;
    // Bind backbuffer RTV with no depth for post-process
    void OMSetBackBufferNoDepth();
    void OMSetBlendState(bool bIsBlendMode) override;
    void Present() override;
	void PSSetDefaultSampler(UINT StartSlot) override;

    void CreateShader(ID3D11InputLayout** OutSimpleInputLayout, ID3D11VertexShader** OutSimpleVertexShader, ID3D11PixelShader** OutSimplePixelShader) override;

    void OnResize(UINT NewWidth, UINT NewHeight);

    void CreateBackBufferAndDepthStencil(UINT width, UINT height);

    void SetViewport(UINT width, UINT height);

    void setviewort(UINT width, UINT height);

    void ResizeSwapChain(UINT width, UINT height);

    void SetFXAAEnabledFlag(bool bEnabled) { bFXAAEnabledFlag = bEnabled; }
    // Allow overriding FXAA params from UI
    void SetFXAAParams(float SubPix, float EdgeThreshold, float EdgeThresholdMin);
private:
    // Compute and upload default FXAA constants from current swapchain size
    void RefreshFXAAConstantsFromSwapchain();

public:
    // getter
    inline ID3D11Device* GetDevice()
    {
        return Device;
    }
    inline ID3D11DeviceContext* GetDeviceContext()
    {
        return DeviceContext;
    }
    inline IDXGISwapChain* GetSwapChain()
    {
        return SwapChain;
    }
    inline ID3D11ShaderResourceView* GetDepthSRV()
    {
        return DepthSRV;
    }
    inline ID3D11ShaderResourceView* GetFXAASRV()
    {
        return FXAASRV;
    }

    // Update viewport CB (b6) from current RS viewport
    void UpdateViewportCBFromCurrent();

private:
    void CreateDeviceAndSwapChain(HWND hWindow)override; // 여기서 디바이스, 디바이스 컨택스트, 스왑체인, 뷰포트를 초기화한다
    void CreateFrameBuffer() override;
    void CreateRasterizerState() override;
    void CreateConstantBuffer() override;
    void CreateDepthStencilState() override;
	void CreateSamplerState();

    // release
	void ReleaseSamplerState();
    void ReleaseBlendState();
    void ReleaseRasterizerState(); // rs
    void ReleaseFrameBuffer(); // fb, rtv
    void ReleaseDeviceAndSwapChain();
 
	void OmSetDepthStencilState(EComparisonFunc Func) override;
    
private:
    //24
    D3D11_VIEWPORT ViewportInfo{};

    //8
    ID3D11Device* Device{};//
    ID3D11DeviceContext* DeviceContext{};//
    IDXGISwapChain* SwapChain{};//

    ID3D11RasterizerState* DefaultRasterizerState{};//
    ID3D11RasterizerState* WireFrameRasterizerState{};//
    ID3D11RasterizerState* FrontCullRasterizerState{};//
    ID3D11RasterizerState* NoCullRasterizerState{};//

    ID3D11DepthStencilState* DepthStencilState{};

    ID3D11DepthStencilState* DepthStencilStateLessEqualWrite = nullptr;      // 기본
    ID3D11DepthStencilState* DepthStencilStateLessEqualReadOnly = nullptr;   // 읽기 전용
    ID3D11DepthStencilState* DepthStencilStateAlwaysNoWrite = nullptr;       // 기즈모/오버레이
    ID3D11DepthStencilState* DepthStencilStateDisable = nullptr;              // 깊이 테스트/쓰기 모두 끔
    ID3D11DepthStencilState* DepthStencilStateGreaterEqualWrite = nullptr;   // 선택사항

    ID3D11BlendState* BlendState{};

    ID3D11Texture2D* FrameBuffer{};//
    ID3D11RenderTargetView* RenderTargetView{};//
    ID3D11DepthStencilView* DepthStencilView{};//
    ID3D11ShaderResourceView* DepthSRV{}; // Depth buffer를 셰이더에서 읽기 위한 SRV

    //FXAA 
    ID3D11Texture2D* FXAATex = nullptr;
    ID3D11RenderTargetView* FXAARTV = nullptr;
    ID3D11ShaderResourceView* FXAASRV = nullptr;

    // 버퍼 핸들
    ID3D11Buffer* ModelCB{};
    ID3D11Buffer* ViewProjCB{};
    ID3D11Buffer* HighLightCB{};
    ID3D11Buffer* BillboardCB{};
    ID3D11Buffer* ColorCB{};
    ID3D11Buffer* PixelConstCB{};
    ID3D11Buffer* UVScrollCB{};
    ID3D11Buffer* InvWorldCB{};
    ID3D11Buffer* ViewportCB{};
    ID3D11Buffer* LightCB{}; 
    ID3D11Buffer* FXAACB{};

    ID3D11Buffer* ConstantBuffer{};

    ID3D11SamplerState* DefaultSamplerState = nullptr;

    bool bFXAAEnabledFlag = true;
    // User override for FXAA parameters (optional)
    bool bFXAAUserParams = false;
    float FXAA_SubPix_User = 0.75f;
    float FXAA_EdgeThreshold_User = 0.125f;
    float FXAA_EdgeThresholdMin_User = 0.0312f;
    
};


template<typename TVertex>
inline HRESULT D3D11RHI::CreateVertexBufferImpl(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer, D3D11_USAGE usage, UINT cpuAccessFlags)
{
    std::vector<TVertex> vertexArray;
    vertexArray.reserve(mesh.Vertices.size());

    for (size_t i = 0; i < mesh.Vertices.size(); ++i) {
        TVertex vtx{};
        vtx.FillFrom(mesh, i);
        vertexArray.push_back(vtx);
    }

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = usage;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = cpuAccessFlags;
    vbd.ByteWidth = static_cast<UINT>(sizeof(TVertex) * vertexArray.size());

    D3D11_SUBRESOURCE_DATA vinitData = {};
    vinitData.pSysMem = vertexArray.data();

    return device->CreateBuffer(&vbd, &vinitData, outBuffer);
}

// PositionColor → Static
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FVertexSimple>(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FVertexSimple>(device, mesh, outBuffer,
        D3D11_USAGE_DEFAULT, 0);
}

// PositionColorTextureNormal → Static
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FVertexDynamic>(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FVertexDynamic>(device, mesh, outBuffer,
        D3D11_USAGE_DEFAULT, 0);
}

// Billboard → Dynamic
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FBillboardVertexInfo_GPU>(ID3D11Device* device, const FMeshData& mesh, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FBillboardVertexInfo_GPU>(device, mesh, outBuffer,
        D3D11_USAGE_DYNAMIC,
        D3D11_CPU_ACCESS_WRITE);
}

template<typename TVertex>
inline HRESULT D3D11RHI::CreateVertexBufferImpl(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer, D3D11_USAGE usage, UINT cpuAccessFlags)
{
    std::vector<TVertex> vertexArray;
    vertexArray.reserve(srcVertices.size());

    for (size_t i = 0; i < srcVertices.size(); ++i)
    {
        TVertex vtx{};
        vtx.FillFrom(srcVertices[i]); // 각 TVertex에서 FillFrom 구현 필요
        vertexArray.push_back(vtx);
    }

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = usage;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = cpuAccessFlags;
    vbd.ByteWidth = static_cast<UINT>(sizeof(TVertex) * vertexArray.size());

    D3D11_SUBRESOURCE_DATA vinitData = {};
    vinitData.pSysMem = vertexArray.data();

    return device->CreateBuffer(&vbd, &vinitData, outBuffer);
}

template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FVertexSimple>(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FVertexSimple>(device, srcVertices, outBuffer, D3D11_USAGE_DEFAULT, 0);
}

// PositionColorTextureNormal
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FVertexDynamic>(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FVertexDynamic>(device, srcVertices, outBuffer, D3D11_USAGE_DEFAULT, 0);
}

// Billboard
template<>
inline HRESULT D3D11RHI::CreateVertexBuffer<FBillboardVertexInfo_GPU>(ID3D11Device* device, const std::vector<FNormalVertex>& srcVertices, ID3D11Buffer** outBuffer)
{
    return CreateVertexBufferImpl<FBillboardVertexInfo_GPU>(device, srcVertices, outBuffer, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
}
