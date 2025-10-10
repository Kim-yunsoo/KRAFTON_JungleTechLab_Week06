// DecalShader.hlsl
// Forward Projection Decal Shader (Without Depth Buffer)
// - X-axis Forward Vector를 기준으로 Projection
// - 영향받는 메쉬의 정점을 직접 처리
// - Decal Box 내부 픽셀만 렌더링

// Mesh World Transform
cbuffer MeshTransformBuffer : register(b0)
{
    row_major float4x4 MeshWorldMatrix; // 메쉬의 월드 변환
}

// View & Projection
cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
}

// Decal Transform
cbuffer DecalTransformBuffer : register(b2)
{
    row_major float4x4 WorldToDecalMatrix; // World → Decal Local Space
}

// Decal Properties
cbuffer DecalPropertiesBuffer : register(b3)
{
    float3 DecalSize;       // Decal Box 크기 (X=Depth, Y=Width, Z=Height)
    float Opacity;          // Fade 적용된 투명도 (0.0 ~ 1.0)

    int BlendMode;          // 0=Translucent, 1=Multiply, 2=Additive
    int bProjectOnBackfaces; // 뒷면 투영 여부
    float2 Padding;
}

Texture2D DecalTexture : register(t0);
SamplerState DecalSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION; // 메쉬의 로컬 정점 위치
    float3 normal : NORMAL0;    // 메쉬의 노멀 (backface culling용)
};

struct PS_INPUT
{
    float4 position : SV_POSITION;        // Clip Space 위치
    float3 decalLocalPos : TEXCOORD0;     // Decal Local Space 위치
    float3 worldNormal : TEXCOORD1;       // 월드 공간 노멀
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // 메쉬 정점을 월드 공간으로 변환
    float4 worldPos = mul(float4(input.position, 1.0f), MeshWorldMatrix);

    // 월드 위치를 Decal Local Space로 변환
    float4 decalLocal = mul(worldPos, WorldToDecalMatrix);
    output.decalLocalPos = decalLocal.xyz / decalLocal.w;

    // Clip Space로 변환 (렌더링용)
    float4x4 VP = mul(ViewMatrix, ProjectionMatrix);
    output.position = mul(worldPos, VP);
    output.position.z -= 0.0001;

    // 노멀을 월드 공간으로 변환
    output.worldNormal = mul(float4(input.normal, 0.0f), MeshWorldMatrix).xyz;
    output.worldNormal = normalize(output.worldNormal);

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Decal Box 범위 체크
    float3 halfSize = DecalSize * 0.5f;

    // Box 외부 픽셀 제거
    if (abs(input.decalLocalPos.x) > halfSize.x ||
        abs(input.decalLocalPos.y) > halfSize.y ||
        abs(input.decalLocalPos.z) > halfSize.z)
    {
        discard;
    }

    // 뒷면 투영 체크 (Optional)
    if (!bProjectOnBackfaces)
    {
        // X > 0인 경우만 투영 (Forward 방향)
        if (input.decalLocalPos.x < 0.0f)
            discard;
    }

    // Forward Projection UV 계산
    // X축: Forward (Projection 방향)
    // Y축: Width  → U
    // Z축: Height → V

    float2 decalUV;
    decalUV.x = (input.decalLocalPos.y + halfSize.y) / DecalSize.y; // Y → U
    decalUV.y = (input.decalLocalPos.z + halfSize.z) / DecalSize.z; // Z → V

    // UV 범위 체크 (0~1)
    if (decalUV.x < 0.0f || decalUV.x > 1.0f ||
        decalUV.y < 0.0f || decalUV.y > 1.0f)
    {
        discard;
    }

    // Decal 텍스처 샘플링
    float4 decalColor = DecalTexture.Sample(DecalSampler, decalUV);

    // Alpha Test
    if (decalColor.a < 0.01f)
        discard;

    // Opacity 적용 (Fade)
    decalColor.a *= Opacity;

    // Blend Mode 적용
    if (BlendMode == 1) // Multiply
    {
        decalColor.rgb = lerp(float3(1, 1, 1), decalColor.rgb, decalColor.a);
    }
    else if (BlendMode == 2) // Additive
    {
        decalColor.rgb *= decalColor.a;
        decalColor.a = 1.0f;
    }

    return decalColor;
}
