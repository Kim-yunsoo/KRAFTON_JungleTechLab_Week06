//======================================================
// ProjectionDecal.hlsl - Mesh-space projection onto actors
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
 
//------------------------------------------------------
// Resources
//------------------------------------------------------
cbuffer DecalBuffer : register(b4)
{
    row_major float4x4 DecalView;
    row_major float4x4 DecalProj;
}

//------------------------------------------------------
// Resources
//------------------------------------------------------
Texture2D g_DecalTexture : register(t0);
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
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    float4 world = mul(float4(input.position, 1.0f), WorldMatrix);
    output.worldPos = world.xyz;
    
    float4 clip = mul(world, mul(ViewMatrix, ProjectionMatrix));
    output.position = clip;
    
    output.position.z -= 0.001f;
    return output;
}

//------------------------------------------------------
// Pixel Shader
//------------------------------------------------------
float4 mainPS(PS_INPUT input) : SV_TARGET
{ 
    float4 decalLocal = mul(float4(input.worldPos, 1.0f), DecalView);
    float4 decalClip  = mul(decalLocal, DecalProj);

    float3 ndc = decalClip.xyz / decalClip.w;
    if (any(abs(ndc.xy) > 1.0))
        discard;
    if(ndc.z > 1.0f || ndc.z < 0.0f)
        discard;
     
    float2 uv = ndc.xy * 0.5f + 0.5f;
    uv.y = 1.0 - uv.y;
    
    float4 decalColor = g_DecalTexture.Sample(g_Sample, uv);
    if (decalColor.a < 0.01f)
        discard;

    return decalColor;
}
