//======================================================
// SceneDepthShader.hlsl - Scene Depth Visualization
//======================================================

//------------------------------------------------------
// Resources
//------------------------------------------------------
Texture2D g_DepthTexture : register(t0);
SamplerState g_Sampler : register(s0);

//------------------------------------------------------
// Constant Buffers
//------------------------------------------------------
cbuffer DepthVisualizationBuffer : register(b6)
{
    float g_NearPlane; // 카메라 Near plane (Depth 선형화용)
    float g_FarPlane; // 카메라 Far plane (Depth 선형화용)
    float2 g_ViewportPos; // Viewport 시작 위치
    float2 g_ViewportSize; // Viewport 크기
    float2 g_ScreenSize; // 전체 화면 크기
    // ✅ 추가: Scene Depth 시각화 범위 (8개의 float = 32 bytes)
    float g_SceneMinDepth; // 시각화 최소 깊이 (예: 1.0)
    float g_SceneMaxDepth; // 시각화 최대 깊이 (예: 100.0)
    float2 g_Padding; // 16바이트 정렬
}

//------------------------------------------------------
// Vertex Shader
//------------------------------------------------------

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

PS_INPUT mainVS(uint vertexID : SV_VertexID)
{
    PS_INPUT output;
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(
        output.texCoord.x * 2.0f - 1.0f,
        -output.texCoord.y * 2.0f + 1.0f,
        0.0f,
        1.0f
    );
    return output;
}

//------------------------------------------------------
// Pixel Shader
//------------------------------------------------------

/**
 * NDC depth를 선형 depth로 변환
 */
float LinearizeDepth(float depthNDC)
{
    // DirectX perspective projection 역변환
    float linearDepth = (2.0f * g_NearPlane * g_FarPlane) /
                        (g_FarPlane + g_NearPlane - depthNDC * (g_FarPlane - g_NearPlane));
    return linearDepth;
}

/**
 * ✅ 선형 깊이를 Scene Depth 범위로 정규화
 */
float NormalizeSceneDepth(float linearDepth)
{
    // [SceneMinDepth, SceneMaxDepth] -> [0, 1]
    float normalizedDepth = (linearDepth - g_SceneMinDepth) / (g_SceneMaxDepth - g_SceneMinDepth);
    return saturate(normalizedDepth); // [0, 1]로 클램핑
}

/**
 * Pixel Shader: Scene Depth 시각화
 */
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // ============================================================
    // 1. Viewport 내 상대 UV를 전체 화면 UV로 변환
    // ============================================================
    float2 viewportStartNormalized = g_ViewportPos / g_ScreenSize;
    float2 viewportSizeNormalized = g_ViewportSize / g_ScreenSize;
    float2 screenUV = viewportStartNormalized + input.texCoord * viewportSizeNormalized;
    
    // ============================================================
    // 2. Depth 샘플링 및 선형화
    // ============================================================
    float depthNDC = g_DepthTexture.Sample(g_Sampler, screenUV).r;
    float linearDepth = LinearizeDepth(depthNDC);
    
    // ============================================================
    // ✅ 3. Scene Depth 범위로 정규화
    // ============================================================
    float normalizedDepth = NormalizeSceneDepth(linearDepth);
    
    // 그레이스케일 출력 (0=검은색(가까움), 1=흰색(멀음))
    return float4(normalizedDepth, normalizedDepth, normalizedDepth, 1.0f);
}