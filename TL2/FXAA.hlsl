// FXAA parameters and integration flags
// Default to PC quality preset; adjust if targeting consoles.
#ifndef FXAA_QUALITY__PRESET
#define FXAA_QUALITY__PRESET 12  // 10~12 are common on PC (higher = better)
#endif

#ifndef FXAA_CONSOLE__
#define FXAA_CONSOLE__ 0
#endif

// If you want to derive luma from green channel or alpha, set here later.
#ifndef FXAA_GREEN_AS_LUMA
#define FXAA_GREEN_AS_LUMA 0
#endif

cbuffer FXAAConstantBuffer : register(b0)
{
    float2 rcpFrame;                 // 1/width, 1/height
    float  FXAASubPix;               // e.g., 0.75
    float  FXAA_Edge_Threshhold;     // e.g., 0.125
    float  FXAA_Edge_Threshhold_Min; // e.g., 0.0312
};

Texture2D ColorLdr : register(t0);
SamplerState LinearClamp : register(s0);

struct VS_OUT
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

VS_OUT VS_FullScreen(uint id: SV_VertexID)
{
    // Fullscreen triangle using SV_VertexID (no vertex buffer needed)
    float2 verts[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    float2 pos = verts[id];
    VS_OUT output;
    output.pos = float4(pos, 0.0f, 1.0f);
    output.uv = 0.5f * float2(pos.x, -pos.y) + 0.5f; 
    return output; 
}

// Step 2) Luma computation helper
float ComputeLuma(float3 rgb)
{
#if FXAA_GREEN_AS_LUMA
    return rgb.g;
#else
    // Rec. 601 luma
    return dot(rgb, float3(0.299, 0.587, 0.114));
#endif
}

float4 PS_FXAA(VS_OUT input) : SV_Target
{
    float3 color = ColorLdr.Sample(LinearClamp, input.uv).rgb;
    float luma = ComputeLuma(color);
    // For now, return original color while we build subsequent steps
    // (edge detection & filtering will follow next steps)
    return float4(color, 1.0f);
}

// Entry point aliases for engine's shader loader
VS_OUT mainVS(uint id : SV_VertexID)
{
    return VS_FullScreen(id);
}

float4 mainPS(VS_OUT input) : SV_Target
{
    return PS_FXAA(input);
}
