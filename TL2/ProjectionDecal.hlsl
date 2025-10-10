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

//------------------------------------------------------
// Vertex Shader
//------------------------------------------------------
PS_INPUT mainVS(VS_INPUT input)
{
    float4 worldPos = mul(float4(input.position, 1.0f), WorldMatrix);
    
    PS_INPUT output;
    output.worldPos = worldPos;
    
    float4 clipSpace = mul(worldPos, mul(ViewMatrix, ProjectionMatrix));
    //float3 ndc = clipSpace.xyz / clipSpace.w;
    
    output.position = clipSpace;
    output.position.z -= 1e-5;
    
    
    return output;
}

//------------------------------------------------------
// Pixel Shader
//------------------------------------------------------
float4 mainPS(PS_INPUT input) : SV_TARGET
{   
    
    float4 decalClipSpace = mul( mul(float4(input.worldPos, 1.0f), DecalView), DecalProj);
    float3 decalNDC = decalClipSpace.xyz / decalClipSpace.w;
    
    if (any(abs(decalNDC.xy) > 1))
        discard;
    
    if(decalNDC.z < 0 || decalNDC.z > 1)
        discard; 
       
    float2 uv = (decalNDC * 0.5f) + 0.5f;
    uv.y = 1 - uv.y;
    
    float4 color = g_DecalTexture.Sample(g_Sample, uv);
    
    return color;
} 