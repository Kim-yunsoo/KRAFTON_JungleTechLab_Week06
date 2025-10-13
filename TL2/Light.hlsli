#ifndef _LIGHT_HLSLI_
#define _LIGHT_HLSLI_

#define POINT_LIGHT 0
#define SPOT_LIGHT 1
#define DIRECTIONAL_LIGHT 2


struct FLinearColor
{   
    float4 Color; 
};

struct FLightInfo
{
    float4 LightPos;

    int Type;
    
    //Point Light
    float Intensity;
    float Radius;
    float RadiusFallOff;
  
    FLinearColor Color;
};

float SoftAttenuate(FLightInfo LightInfo, float4 Position)
{ 
    float4 d = length(Position - LightInfo.LightPos);
    float x = saturate(d / max(LightInfo.Radius, 1e-4));   
    return pow(1.0f - x, max(LightInfo.RadiusFallOff, 1e-4));
}

float4 Calculate_PointLight(FLightInfo LightInfo)
{
    // 감쇠 구하기
    
    // diffuse, specular 구하기
    
    // (diffuse + specular) * Color * 감쇠
    return float4(1, 1, 1, 1);
}

#endif // _LIGHT_HLSLI_