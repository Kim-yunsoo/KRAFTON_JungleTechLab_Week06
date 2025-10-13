//======================================================
// SceneDepthShader.hlsl - Scene Depth Visualization
//======================================================

//------------------------------------------------------
// Resources
//------------------------------------------------------
Texture2D g_DepthTexture : register(t0); // Depth buffer SRV
SamplerState g_Sampler : register(s0); // 텍스처 샘플러

//------------------------------------------------------
// Constant Buffers
//------------------------------------------------------
cbuffer DepthVisualizationBuffer : register(b6)
{
    float g_NearPlane; // 카메라 Near plane
    float g_FarPlane; // 카메라 Far plane
    float2 g_Padding;
}

//------------------------------------------------------
// Vertex Shader
//------------------------------------------------------

struct PS_INPUT
{
    float4 position : SV_POSITION; // 클립 공간 좌표
    float2 texCoord : TEXCOORD0; // UV 좌표
};

/**
 * Vertex Shader: 버퍼 없이 SV_VertexID만으로 전체 화면 삼각형 생성
 * Draw(3, 0) 호출 시 3개 정점으로 화면을 덮는 큰 삼각형 생성
 */
PS_INPUT mainVS(uint vertexID : SV_VertexID)
{
    PS_INPUT output;
    
    // 비트 연산으로 UV 좌표 생성: (0,0), (2,0), (0,2)
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    
    // UV [0,2]를 NDC [-1,1]로 변환하고 Y축 플립
    output.position = float4(
        output.texCoord.x * 2.0f - 1.0f, // X: [0,2] -> [-1,3]
        -output.texCoord.y * 2.0f + 1.0f, // Y: [0,2] -> [1,-3] (플립)
        0.0f, // Near plane
        1.0f // W = 1
    );
    
    return output;
}

//------------------------------------------------------
// Pixel Shader
//------------------------------------------------------

/**
 * NDC depth를 선형 depth로 변환
 * DirectX의 비선형 depth 분포를 선형으로 펼쳐서 시각화
 */
float LinearizeDepth(float depthNDC)
{
    // DirectX perspective projection 역변환
    // linearDepth = (2 * near * far) / (far + near - depthNDC * (far - near))
    float linearDepth = (2.0f * g_NearPlane * g_FarPlane) /
                        (g_FarPlane + g_NearPlane - depthNDC * (g_FarPlane - g_NearPlane));
    
    // [near, far] 범위를 [0, 1]로 정규화
    return (linearDepth - g_NearPlane) / (g_FarPlane - g_NearPlane);
}

/**
 * Pixel Shader: Depth 값을 선형화하여 그레이스케일로 시각화
 * 가까운 객체(0.0) = 검은색, 먼 객체(1.0) = 흰색
 */
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Depth 텍스처에서 NDC 깊이 값 샘플링 (비선형)
    float depthNDC = g_DepthTexture.Sample(g_Sampler, input.texCoord).r;
    
    // NDC depth를 선형 depth로 변환
    float linearDepth = LinearizeDepth(depthNDC);
    
    // 선형 깊이를 그레이스케일로 출력
    return float4(linearDepth, linearDepth, linearDepth, 1.0f);
}