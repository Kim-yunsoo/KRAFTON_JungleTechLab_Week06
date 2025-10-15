#pragma once
#include "BillboardComponent.h"
#include "RHIDevice.h"
#include "LineDynamicMesh.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMeshComponent;
class URHIDevice;
class UShader;
class UStaticMesh;
class D3D11RHI;
struct FMaterialSlot;

class URenderer
{
public:
    URenderer(URHIDevice* InDevice);

    ~URenderer();

public:
	void BeginFrame();

    void PrepareShader(FShader& InShader);

    void PrepareShader(UShader* InShader);

    void OMSetBlendState(bool bIsChecked);

    void RSSetState(EViewModeIndex ViewModeIndex);

    void RSSetFrontCullState();

    void RSSetNoCullState();

    void RSSetDefaultState();

    void UpdateConstantBuffer(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix);

    void UpdateHighLightConstantBuffer(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo);

    void UpdateBillboardConstantBuffers(const FVector& pos, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, const FVector& CameraRight, const FVector& CameraUp);

    void UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture);

    void UpdateColorBuffer(const FVector4& Color);

    void UpdateInvWorldBuffer(const FMatrix& InvWorldMatrix, const FMatrix& InvViewProjMatrix);

    void UpdateViewportBuffer(float StartX, float StartY, float SizeX, float SizeY);

    void UpdateViewportBuffer(float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight, float ScreenWidth, float ScreenHeight);

    void UpdateDepthVisualizationBuffer(float NearPlane, float FarPlane, float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight, float ScreenWidth, float ScreenHeight);

    void UpdateCameraNearFarBuffer(float NearPlane, float FarPlane);

    void UpdateFogParameterBuffer(float FogDensity, float FogHeightFalloff, float FogStartDistance, float FogCutoffDistance, float FogMaxOpacity, const FVector4& FogInscatteringColor, const FVector& FogComponentPosition);

    void UpdateInverseViewProjMatrixBuffer(const FMatrix& InvViewMatrix, const FMatrix& InvProjectionMatrix);

    void UpdateCopyShaderViewportBuffer(float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight, float ScreenWidth, float ScreenHeight);

    void DrawIndexedPrimitiveComponent(UStaticMesh* InMesh, D3D11_PRIMITIVE_TOPOLOGY InTopology, const TArray<FMaterialSlot>& InComponentMaterialSlots);

    void UpdateUVScroll(const FVector2D& Speed, float TimeSec);

    void DrawIndexedPrimitiveComponent(UTextRenderComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology);
    void DrawIndexedPrimitiveComponent(UBillboardComponent* Comp,
                                       D3D11_PRIMITIVE_TOPOLOGY InTopology);

    void SetViewModeType(EViewModeIndex ViewModeIndex);
    // Batch Line Rendering System
    void BeginLineBatch();
    void AddLine(const FVector& Start, const FVector& End, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    void AddLines(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors);
    void EndLineBatch(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix);
    void ClearLineBatch();

	void EndFrame();

    void OMSetDepthStencilState(EComparisonFunc Func);

    URHIDevice* GetRHIDevice() { return RHIDevice; }

    // Lighting: per-frame visible lights cache and upload
    void SetWorldLights(const TArray<FLightInfo>& InLights);
    const TArray<FLightInfo>& GetWorldLights() const;
    void UpdateLightBuffer();
    void UpdateLightBuffer(const TArray<FLightInfo>& InLights);
     
    // Anti-aliasing toggles
    void SetFXAAEnabled(bool bEnabled);
    void SetFXAAParams(float SpanMax, float ReduceMul, float ReduceMin);

    // 현재 FXAA 파라미터를 Constant Buffer에 바인딩
	void BindCurrentFXAAParams();

    bool IsFXAAEnabled() const { return bFXAAEnabled; }

    int GetFXAAQuality() { return FXAAVersion; }
    void SetFXAAQuality(int Version) { FXAAVersion = Version; }

    void PostProcessing();


    // ✅ Scene RenderTarget 관리
    void BeginSceneRendering();
    void EndSceneRendering();

private:
    URHIDevice* RHIDevice;

    // Batch Line Rendering System using UDynamicMesh for efficiency
    ULineDynamicMesh* DynamicLineMesh = nullptr;
    FMeshData* LineBatchData = nullptr;
    UShader* LineShader = nullptr;
    bool bLineBatchActive = false;
    static const uint32 MAX_LINES = 10000;  // Maximum lines per batch
    
    // 렌더링 통계를 위한 상태 추적
    UMaterial* LastMaterial = nullptr;
    UShader* LastShader = nullptr;
    UTexture* LastTexture = nullptr;

    /**
     * @brief 불필요한 API 호출을 막기 위해 마지막으로 바인딩된 상태를 캐싱합니다.
     */
    ID3D11Buffer* LastVertexBuffer = nullptr;
    ID3D11Buffer* LastIndexBuffer = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY LastPrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11ShaderResourceView* LastTextureSRV = nullptr;

    void InitializeLineBatch();
    void ResetRenderStateTracking();

    // Visible Light
    TArray<FLightInfo> WorldLights;

    // 마지막 설정된 FXAA 파라미터 캐싱
    struct FFXAAParams
    {
        float SpanMax = 8.0f;
        float ReduceMul = 1.0f / 8.0f;
        float ReduceMin = 1.0f / 128.0f;
    } CachedFXAAParams;

    // Post-process FXAA shader
    UShader* FXAAShader = nullptr;
    bool bFXAAEnabled = false;
    int FXAAVersion = 1; 
};

