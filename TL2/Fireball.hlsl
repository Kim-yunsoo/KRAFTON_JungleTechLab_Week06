// ============================================================================
// FireTint.hlsl  (Surface-based red/yellow tint with FBM wobble; no raymarch)
// ============================================================================

#include "Light.hlsli"

// ===== Constant Buffers =====
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

struct FMaterial
{
    float3 DiffuseColor; // Kd
    float OpticalDensity; // Ni

    float3 AmbientColor; // Ka
    float Transparency; // Tr or d

    float3 SpecularColor; // Ks
    float SpecularExponent; // Ns

    float3 Emissifloatolor; // Ke (spelling kept as provided)
    uint IlluminationModel; // illum

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
    bool HasTexture; // 4 bytes
}

cbuffer PSScrollCB : register(b5)
{
    float2 UVScrollSpeed;
    float UVScrollTime; // Reused as FireTime for fire tint
    float _pad_scrollcb;
}

// ===== IO Structures =====
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
    float3 normal : NORMAL0;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

// ===== Resources =====
Texture2D g_DiffuseTexColor : register(t0);
SamplerState g_Sample : register(s0);

// ===== Noise / FBM =====
static const float DENSITY = 4.5;

float hash(float3 p3)
{
    p3 = frac(p3 * float3(0.1031, 0.11369, 0.13787));
    p3 += dot(p3, p3.yzx + 19.19);
    return frac((p3.x + p3.y) * p3.z);
}

float3 grad(float3 p)
{
    return normalize(-1.0 + 2.0 * float3(hash(p.xyz), hash(p.yxy), hash(p.zyx)));
}

float3 fade(float3 p, float3 corner)
{
    float3 t = abs(p - corner);
    return 1.0 - (6.0 * t * t * t * t * t - 15.0 * t * t * t * t + 10.0 * t * t * t);
}

float perlin(float3 p)
{
    p *= DENSITY;
    float3 min_corner = floor(p);
    float3 local = frac(p);
    float ret = 0.0;

    [unroll]
    for (int dx = 0; dx <= 1; ++dx)
    {
        [unroll]
        for (int dy = 0; dy <= 1; ++dy)
        {
            [unroll]
            for (int dz = 0; dz <= 1; ++dz)
            {
                float3 corner = min_corner + float3(dx, dy, dz);
                float3 g = grad(corner);
                float3 d = local - float3(dx, dy, dz);
                float3 f = fade(p, corner);
                ret += dot(g, d) * f.x * f.y * f.z;
            }
        }
    }
    return ret;
}

float fbm(float3 p)
{
    float ret = 0.0;
    float amp = 1.0;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        ret += amp * perlin(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return ret;
}

// ===== Shaders =====
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    float4x4 VP = mul(ViewMatrix, ProjectionMatrix);
    float4 wp = mul(float4(input.position, 1.0f), WorldMatrix);

    output.position = mul(wp, VP);
    output.color = input.color;

    // w = 0 -> treat as direction, no translation
    output.normal = normalize(mul(float4(input.normal, 0.0f), WorldMatrix).xyz);
    output.worldPos = wp.xyz;
    output.texCoord = input.texCoord;

    // Gizmo axis color override (kept from original)
    if (GIzmo == 1)
    {
        float4 c = output.color;
        if (Y == 1)
            c = float4(1.0, 1.0, 0.0, c.a); // Yellow
        else if (X == 1)
            c = float4(1.0, 0.0, 0.0, c.a); // Red
        else if (X == 2)
            c = float4(0.0, 1.0, 0.0, c.a); // Green
        else if (X == 3)
            c = float4(0.0, 0.0, 1.0, c.a); // Blue
        output.color = c;
    }

    return output;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    // Base color/mat/texture
    float4 finalColor = input.color;
    finalColor.rgb = lerp(finalColor.rgb, LerpColor.rgb, LerpColor.a) * (1.0f - HasMaterial);

    if (HasMaterial && HasTexture)
    {
        float2 uv = input.texCoord + UVScrollSpeed * UVScrollTime;
        finalColor.rgb = g_DiffuseTexColor.Sample(g_Sample, uv);
    }

    // Pick highlight (kept)
    if (Picked == 1)
    {
        float3 highlightColor = float3(1.0, 1.0, 0.0);
        finalColor.rgb = lerp(finalColor.rgb, highlightColor, 0.5);
    }

    // Minimal ambient (kept)
    float3 N = normalize(input.normal);
    float3 lighting = finalColor.rgb * 0.4f;
    finalColor.rgb = saturate(finalColor.rgb * lighting);

    // ---- Fire tint: surface-based wobble (no raymarch) ----
    // Use existing PSScrollCB to avoid extra per-material binding
    const float FireNoiseScale = 0.45f;
    const float FireNoiseSpeed = 0.45f;
    const float FireTintStrength = 0.6f; // [0,1]
    const float3 FireColorA = float3(1.0f, 0.19f, 0.0f);
    const float3 FireColorB = float3(1.0f, 0.91f, 0.0f);

    float3 p = input.worldPos * FireNoiseScale + UVScrollTime * FireNoiseSpeed;
    
    // fbm ~ [-1,1] -> remap to [0,1]
    float n = fbm(p);
    n = saturate(n * 0.5f + 0.5f);
    
    // red -> yellow palette
    float3 fireTint = lerp(FireColorA, FireColorB, n);
    
    // mix onto base
    finalColor.rgb = lerp(finalColor.rgb, fireTint, saturate(FireTintStrength));
    
    return finalColor;
}
