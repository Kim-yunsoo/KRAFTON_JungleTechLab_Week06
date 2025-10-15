//======================================================
// CopyShader.hlsl - Simple Texture Copy (Viewport-aware)
//======================================================

//------------------------------------------------------
// Resources
//------------------------------------------------------
Texture2D g_SourceTexture : register(t0); // Scene Color Texture
SamplerState g_Sampler : register(s0);

//------------------------------------------------------
// Constant Buffers
//------------------------------------------------------
cbuffer ViewportBuffer : register(b7)
{
    float2 g_ViewportPos; // Viewport 시작 위치 (픽셀 단위)
    float2 g_ViewportSize; // Viewport 크기 (픽셀 단위)
    float2 g_ScreenSize; // 전체 화면 크기 (픽셀 단위)
    float2 g_Padding; // 16바이트 정렬을 위한 패딩
}

//------------------------------------------------------
// Vertex Shader
//------------------------------------------------------
struct PS_INPUT
{
    float4 position : SV_POSITION; // 클립 공간 좌표
    float2 texCoord : TEXCOORD0; // UV 좌표 [0,1] (Viewport 내 상대 좌표)
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
 * Pixel Shader: Viewport 영역의 Scene Texture를 BackBuffer로 복사
 */
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // ============================================================
    // 1. Viewport 내 상대 UV [0,1]을 전체 화면 UV로 변환
    // ============================================================
    
    // Viewport의 시작 위치를 [0,1] 범위로 정규화
    float2 viewportStartNormalized = g_ViewportPos / g_ScreenSize;
    
    // Viewport의 크기를 [0,1] 범위로 정규화
    float2 viewportSizeNormalized = g_ViewportSize / g_ScreenSize;
    
    // Viewport 내 상대 좌표 [0,1]을 전체 화면 좌표로 변환
    float2 screenUV = viewportStartNormalized + input.texCoord * viewportSizeNormalized;
    
    // ============================================================
    // 2. Scene Texture에서 샘플링
    // ============================================================
    float4 sceneColor = g_SourceTexture.Sample(g_Sampler, screenUV);
    
    return sceneColor;
}