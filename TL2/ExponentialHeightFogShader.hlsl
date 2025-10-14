//======================================================
// ExponentialHeightFogShader.hlsl - Height Fog Post-Process
//======================================================

//------------------------------------------------------
// Resources
//------------------------------------------------------
Texture2D g_SceneTexture : register(t0); // 렌더링된 씬 텍스처
Texture2D g_DepthTexture : register(t1); // Depth buffer (선형화 필요)
SamplerState g_Sampler : register(s0); // 텍스처 샘플러

//------------------------------------------------------
// Constant Buffers
//------------------------------------------------------

// b0: Camera 정보 (Depth 선형화용)
cbuffer CameraBuffer : register(b0)
{
    float g_NearPlane; // 카메라 Near plane
    float g_FarPlane; // 카메라 Far plane
    float2 g_Padding0; // 16바이트 정렬
}

// b1: Fog 파라미터
cbuffer FogParameterBuffer : register(b1)
{
    float g_FogDensity; // 안개 전역 밀도
    float g_FogHeightFalloff; // 높이에 따른 밀도 감소율
    float g_FogStartDistance; // 안개 시작 거리
    float g_FogCutoffDistance; // 안개 최대 거리
    
    float g_FogMaxOpacity; // 안개 최대 불투명도
    float3 g_Padding1; // 16바이트 정렬
    
    float4 g_FogInscatteringColor; // 안개 산란 색상 (RGB + Alpha)
    
    float3 g_FogComponentPosition; // HeightFogComponent의 월드 위치 (안개 기준 높이)
    float g_Padding2; // 16바이트 정렬
}

// b2: View-Projection 역행렬 (World Position 복원용)
cbuffer InverseMatrixBuffer : register(b2)
{
    row_major float4x4 g_InvViewMatrix; // View 역행렬
    row_major float4x4 g_InvProjectionMatrix; // Projection 역행렬
}

// b3: Viewport 정보
cbuffer ViewportBuffer : register(b3)
{
    float2 g_ViewportPos; // Viewport 시작 위치 (픽셀)
    float2 g_ViewportSize; // Viewport 크기 (픽셀)
    float2 g_ScreenSize; // 전체 화면 크기 (픽셀)
    float2 g_Padding3; // 16바이트 정렬
}

//------------------------------------------------------
// Vertex Shader
//------------------------------------------------------

struct PS_INPUT
{
    float4 position : SV_POSITION; // 클립 공간 좌표
    float2 texCoord : TEXCOORD0; // UV 좌표 [0,1]
};

/**
 * Vertex Shader: Fullscreen Triangle 생성
 * Draw(3, 0) 호출 시 화면을 덮는 큰 삼각형 생성
 */
PS_INPUT mainVS(uint vertexID : SV_VertexID)
{
    PS_INPUT output;
    
    // 비트 연산으로 UV 좌표 생성: (0,0), (2,0), (0,2)
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    
    // UV [0,2]를 NDC [-1,1]로 변환하고 Y축 플립
    output.position = float4(
        output.texCoord.x * 2.0f - 1.0f, // X: [0,2] -> [-1,3]
        -output.texCoord.y * 2.0f + 1.0f, // Y: [0,2] -> [1,-3]
        0.0f, // Near plane
        1.0f // W = 1
    );
    
    return output;
}

//------------------------------------------------------
// Helper Functions
//------------------------------------------------------

/**
 * NDC Depth를 선형 Depth로 변환
 */
float LinearizeDepth(float depthNDC)
{
    // DirectX perspective projection 역변환
    float linearDepth = (2.0f * g_NearPlane * g_FarPlane) /
                        (g_FarPlane + g_NearPlane - depthNDC * (g_FarPlane - g_NearPlane));
    return linearDepth;
}

/**
 * Screen UV와 Depth로부터 World Position 복원
 */
float3 ReconstructWorldPosition(float2 screenUV, float depthNDC)
{
    // 1. NDC 좌표 계산 (UV [0,1] -> NDC [-1,1])
    float2 ndcXY = screenUV * 2.0f - 1.0f;
    ndcXY.y = -ndcXY.y; // Y축 플립 (UV와 NDC의 Y축 방향이 반대)
    
    // 2. Clip Space 좌표 생성 (w = 1.0 가정)
    float4 clipPos = float4(ndcXY, depthNDC, 1.0f);
    
    // 3. View Space로 변환 (Projection 역행렬)
    float4 viewPos = mul(clipPos, g_InvProjectionMatrix);
    viewPos /= viewPos.w; // Perspective divide
    
    // 4. World Space로 변환 (View 역행렬)
    float4 worldPos = mul(viewPos, g_InvViewMatrix);
    
    return worldPos.xyz;
}

/**
 * 특정 높이에서의 안개 밀도 계산 (Exponential Height Fog)
 */
float CalculateFogDensityAtHeight(float worldHeight)
{
    // 안개 컴포넌트 높이와의 차이
    float heightDifference = worldHeight - g_FogComponentPosition.z;
    
    // 지수 함수로 높이에 따른 밀도 감소
    // Density(h) = GlobalDensity * exp(-HeightFalloff * (h - FogHeight))
    float densityAtHeight = g_FogDensity * exp(-g_FogHeightFalloff * heightDifference);
    
    return max(0.0f, densityAtHeight);
}

/**
 * 카메라에서 특정 지점까지의 안개 적용량 계산
 */
float CalculateFogAmount(float3 cameraPos, float3 worldPos, float distance)
{
    // 1. StartDistance 이전에는 안개 없음
    if (distance < g_FogStartDistance)
        return 0.0f;
    
    // 2. CutoffDistance 이후에는 최대 불투명도
    if (distance > g_FogCutoffDistance)
        return g_FogMaxOpacity;
    
    // 3. 거리 기반 안개 계산 [0, 1]
    float distanceFactor = (distance - g_FogStartDistance) / (g_FogCutoffDistance - g_FogStartDistance);
    distanceFactor = saturate(distanceFactor);
    
    // 4. 높이 기반 안개 밀도 (중간 지점의 높이 사용)
    float midPointHeight = (cameraPos.z + worldPos.z) * 0.5f;
    float heightDensity = CalculateFogDensityAtHeight(midPointHeight);
    
    // 5. 최종 안개 적용량 = 거리 * 높이 밀도 * 최대 불투명도
    float fogAmount = distanceFactor * heightDensity * g_FogMaxOpacity;
    
    return saturate(fogAmount);
}

//------------------------------------------------------
// Pixel Shader
//------------------------------------------------------

/**
 * Pixel Shader: Exponential Height Fog 적용
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
    // 2. 씬 텍스처와 Depth 샘플링
    // ============================================================
    float4 sceneColor = g_SceneTexture.Sample(g_Sampler, screenUV);
    float depthNDC = g_DepthTexture.Sample(g_Sampler, screenUV).r;
    
    // ============================================================
    // 3. World Position 복원
    // ============================================================
    float3 worldPos = ReconstructWorldPosition(screenUV, depthNDC);
    
    // 카메라 위치 (View 역행렬의 마지막 행 = Translation)
    float3 cameraPos = float3(g_InvViewMatrix[3][0], g_InvViewMatrix[3][1], g_InvViewMatrix[3][2]);
    
    // 카메라에서 픽셀까지의 거리
    float distance = length(worldPos - cameraPos);
    
    // ============================================================
    // 4. 안개 적용량 계산
    // ============================================================
    float fogAmount = CalculateFogAmount(cameraPos, worldPos, distance);
    
    // ============================================================
    // 5. 최종 색상: 씬 색상과 안개 색상 블렌딩
    // ============================================================
    // finalColor = lerp(sceneColor, fogColor, fogAmount)
    float3 finalColor = lerp(sceneColor.rgb, g_FogInscatteringColor.rgb, fogAmount);
    
    return float4(finalColor, sceneColor.a);
}