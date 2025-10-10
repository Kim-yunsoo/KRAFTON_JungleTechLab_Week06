//======================================================
// DecalShader.hlsl - Screen-Space Decal Shader (Fixed)
//======================================================

cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
}

cbuffer ColorBuffer : register(b3)
{
    float4 LerpColor;
}

cbuffer InvWorldBuffer : register(b4)
{
    row_major float4x4 InvWorldMatrix;
    row_major float4x4 InvViewProjMatrix;
}

cbuffer ViewportBuffer : register(b6)
{
    float4 ViewportRect; // x=StartX, y=StartY, z=SizeX, w=SizeY
}

//------------------------------------------------------
// Resources
//------------------------------------------------------
Texture2D g_DecalTexture : register(t0);
Texture2D g_DepthTexture : register(t1);
SamplerState g_Sample : register(s0);

//------------------------------------------------------
// Vertex Shader
//------------------------------------------------------
struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL0;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // 픽셀 셰이더에서 자동으로 화면 픽셀 좌표로 변환됨
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // 월드 → 뷰 → 프로젝션 (row_major 기준)
    float4x4 MVP = mul(mul(WorldMatrix, ViewMatrix), ProjectionMatrix);
    output.position = mul(float4(input.position, 1.0f), MVP);

    return output;
}

//------------------------------------------------------
// Pixel Shader
//------------------------------------------------------
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // 1️⃣ Depth 샘플링: 전체 화면 기준 UV 사용
    float2 screenSize;
    g_DepthTexture.GetDimensions(screenSize.x, screenSize.y);
    float2 depthUV = input.position.xy / screenSize;

    // 2️⃣ Depth 버퍼 샘플링 (0~1)
    float depth = g_DepthTexture.Sample(g_Sample, depthUV).r;

    // 3️⃣ NDC 계산: 뷰포트 로컬 UV 사용
    float2 viewportLocalPos = input.position.xy - ViewportRect.xy;
    float2 viewportUV = viewportLocalPos / ViewportRect.zw;
    
      // 🐛 디버그: 깊이 값 시각화
    //return float4(depth, depth, depth, 1.0f);
    // 4️⃣ NDC 좌표 구성 (DirectX: Z 0~1 → NDC -1~1 변환)
    float4 ndcPos;
    ndcPos.xy = viewportUV * 2.0f - 1.0f;
    ndcPos.y = -ndcPos.y; // Y축 반전 (UV는 아래로 증가, NDC는 위로 증가)
    ndcPos.z = depth ; // ✅ 중요 수정점
    ndcPos.w = 1.0f;
      //return ndcPos;
    // 4️⃣ NDC → World
    float4 worldPos = mul(ndcPos, InvViewProjMatrix);
    worldPos /= worldPos.w;

    // 5️⃣ World → Decal Local
    float3 decalLocalPos = mul(worldPos, InvWorldMatrix).xyz;
    
    // 6️⃣ 데칼 박스 범위 검사 (-0.5~+0.5)
    // 데칼 박스 밖의 픽셀은 렌더링하지 않음
    if (abs(decalLocalPos.x) > 1.0f ||
        abs(decalLocalPos.y) > 1.0f ||
        abs(decalLocalPos.z) > 1.0f)
    {
        discard;
    }
   
    //return float4(1,0,0,1);
    
    // 7️⃣ 로컬 → UV (0~1)
    float2 decalUV = decalLocalPos.xy + 0.5f;

  

    // 🐛 디버그: 로컬 좌표 시각화
    // return float4(decalLocalPos * 0.5f + 0.5f, 1.0f);

    // 🐛 디버그: UV 좌표 시각화
     //return float4(decalUV, 0.0f, 1.0f);

    // 8️⃣ 데칼 텍스처 샘플링
    float4 decalColor = g_DecalTexture.Sample(g_Sample, decalUV);

    // 9️⃣ 알파 컷아웃
    if (decalColor.a < 0.01f)
        discard;
   // return float4(decalLocalPos * 0.5f + 0.5f, 1.0f);
    // 🔟 최종 색상 출력 (하드웨어 블렌딩)
    return decalColor;
}
