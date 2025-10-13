#ifndef _LIGHT_HLSLI_
#define _LIGHT_HLSLI_

#define POINT_LIGHT 0
#define SPOT_LIGHT 1
#define DIRECTIONAL_LIGHT 2

#define MAX_LIGHTS 8
 

struct FLightInfo
{
    int Type;
    float3 LightPos;

    float4 Color;
    
    //Point Light
    float Intensity;
    float Radius;
    float RadiusFallOff;
    float Padding;
    
    float4 LightDir;
};


cbuffer LightBuffer : register(b7)
{
    FLightInfo Lights[MAX_LIGHTS];
}

float SoftAttenuate(FLightInfo LightInfo, float3 Position)
{
    float dist = length(Position - LightInfo.LightPos);
    float x = saturate(dist / LightInfo.Radius);
    return pow(1.0f - x, LightInfo.RadiusFallOff);
}

// Diffuse-only point light helper (world-space)
float3 Calculate_PointLight_Diffuse(FLightInfo LightInfo, float3 worldPos, float3 worldNormal)
{
    //LightInfo.LightPos = float3(0, 0, 0);

    float3 L = normalize(LightInfo.LightPos - worldPos);
    float NdotL = max(dot(worldNormal, L),1e-3);
    
    float atten = SoftAttenuate(LightInfo, worldPos);
    
    float3 lightColor = LightInfo.Color.rgb * LightInfo.Intensity;

    return lightColor * NdotL * atten;
} 
#endif // _LIGHT_HLSLI_
