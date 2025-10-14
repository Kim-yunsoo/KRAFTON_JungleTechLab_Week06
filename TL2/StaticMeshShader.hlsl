#include "Light.hlsli"

cbuffer ModelBuffer : register(b0)
{
    row_major float4x4 WorldMatrix;
}

cbuffer ViewProjBuffer : register(b1)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
}

cbuffer HighLightBuffer : register(b2)
{
    int Picked;
    float3 Color;
    int X;
    int Y;
    int Z;
    int GIzmo;
}

struct VS_INPUT
{
    float3 position : POSITION; // Input position from vertex buffer
    float3 normal : NORMAL0;
    float4 color : COLOR; // Input color from vertex buffer
    float2 texCoord : TEXCOORD0;
};


Texture2D g_DiffuseTexColor : register(t0);
SamplerState g_Sample : register(s0);

struct FMaterial
{
    float3 DiffuseColor; // Kd
    float OpticalDensity; // Ni
    
    float3 AmbientColor; // Ka
    float Transparency; // Tr Or d
    
    float3 SpecularColor; // Ks
    float SpecularExponent; // Ns
    
    float3 EmissiveColor; // Ke
    uint IlluminationModel; // illum. Default illumination model to Phong for non-Pbr materials
    
    float3 TransmissionFilter; // Tf  
    float dummy;
};

cbuffer ColorBuffer : register(b3)
{
    float4 LerpColor;
}

cbuffer PixelConstData : register(b4)
{
    FMaterial Material;
    bool HasMaterial; // 4 bytes
    bool HasTexture;
}
cbuffer PSScrollCB : register(b5)
{
    float2 UVScrollSpeed;
    float  UVScrollTime;
    float  _pad_scrollcb;
}

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float3 normal : NORMAL0;
    float4 color : COLOR; // Color to pass to the pixel shader
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    // 상수버퍼를 통해 넘겨 받은 Offset을 더해서 버텍스를 이동 시켜 픽셀쉐이더로 넘김
    // float3 scaledPosition = input.position.xyz * Scale;
    // output.position = float4(Offset + scaledPosition, 1.0);
    
    float4x4 MVP = mul(mul(WorldMatrix, ViewMatrix), ProjectionMatrix); 
    float4 worldPos = mul(float4(input.position, 1.0f), WorldMatrix);
    output.position = mul(worldPos, mul(ViewMatrix, ProjectionMatrix));
    
    
    // change color
    float4 c = input.color;
    
    if (GIzmo == 1)
    {
        if (Y == 1)
        {
            c = float4(1.0, 1.0, 0.0, c.a); // Yellow
        }
        else
        {
            if (X == 1)
                c = float4(1.0, 0.0, 0.0, c.a); // Red
            else if (X == 2)
                c = float4(0.0, 1.0, 0.0, c.a); // Green
            else if (X == 3)
                c = float4(0.0, 0.0, 1.0, c.a); // Blue
        }
        
    }
    
    
    // Pass the color to the pixel shader
    output.color = c;
     
    // w를 0으로 초기화하면, vector이기 때문에 Translation이 무시된다.
    output.normal = normalize(mul(float4(input.normal, 0.0f), WorldMatrix).xyz); 
    //output.worldPos = worldPos.zx; 
    output.worldPos = worldPos.xyz;
    
    output.texCoord = input.texCoord;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Lerp the incoming color with the global LerpColor
    float4 finalColor = input.color;
    finalColor.rgb = lerp(finalColor.rgb, LerpColor.rgb, LerpColor.a) * (1.0f - HasMaterial);
    //finalColor.rgb += Material.DiffuseColor * HasMaterial;

    if (HasMaterial && HasTexture)
    {
        float2 uv = input.texCoord + UVScrollSpeed * UVScrollTime;
        finalColor.rgb = g_DiffuseTexColor.Sample(g_Sample, uv);
    }
    if (Picked == 1)
    {
        // preserve gizmo highlight blending
        float3 highlightColor = float3(1.0, 1.0, 0.0);
        finalColor.rgb = lerp(finalColor.rgb, highlightColor, 0.5);
    } 
    
    // Simple forward lighting (diffuse only)
    float3 N = normalize(input.normal);
    float3 worldPos = input.worldPos;
    float3 lighting = float3(0.0f, 0.0f, 0.0f);
    
    float3 ambientColor = finalColor * 0.4f ;
    lighting = ambientColor;
    
    [unroll]
    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        lighting += Calculate_PointLight_Diffuse(Lights[i], worldPos, N);    
    } 
    
    finalColor.rgb = saturate(finalColor.rgb * lighting);
    
    return finalColor;
}
