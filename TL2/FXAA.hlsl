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

float4 PS_FXAA(VS_OUT input) : SV_Target
{
    return float4(ColorLdr.Sample(LinearClamp, input.uv).rgb, 1.0f);

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
