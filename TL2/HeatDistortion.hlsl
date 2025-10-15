cbuffer HeatConstantBuffer : register(b8)
{
    int NumSpots;
    float DistortionPx;
    float EmissiveMul;
    float Time;
    
    float2 InvRes;
    float FalloffExp;
    float Padding;
    
    // xy: uv, z: RadiusPx, w: Strength
    float4 Spots[8];
};

// Optional viewport input: x=StartX, y=StartY, z=SizeX, w=SizeY (pixels)
cbuffer HeatViewportCB : register(b6)
{
    float4 ViewportRect;
}

Texture2D SceneColor : register(t0);
Texture2D NoiseTex : register(t1);
SamplerState Sampler : register(s0);

struct VS_OUT
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
}; 

VS_OUT mainVS(uint id: SV_VertexID)
{
    float2 verts[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0)
    };

    float2 pos = verts[id];
    VS_OUT output;
    output.pos = float4(pos, 0.0f, 1.0f);
    output.uv = 0.5f * float2(pos.x, -pos.y) + 0.5f;
    return output;
}

float4 mainPS(VS_OUT input) : SV_Target
{ 
    float2 viewportLocalPos = input.pos.xy - ViewportRect.xy;
    float2 uv = (ViewportRect.xy + viewportLocalPos) * InvRes;
    
    float2 px = InvRes;
    
    float2 timeUV = uv + float2(Time * 1.2, Time * 1.5);
    float2 noise = NoiseTex.Sample(Sampler, timeUV); // [0 , 1]
    noise = noise * 2 - 1; // [-1, 1]
    
    float2 offset = 0;
    float glow = 0;
    
    [loop]
    for (uint s = 0; s < NumSpots; ++s)
    {
        // Spots[s].xy is the LOCAL UV of the heat spot. Convert it to global UV.
        float2 cUV_local = Spots[s].xy;
        float2 cUV_global = (ViewportRect.xy + cUV_local * ViewportRect.zw) * InvRes;

        float rpx = max(Spots[s].z, 1.0);
        float k = Spots[s].w;   

        // uv→픽셀좌표로 변환해 반경(px)으로 감쇠
        float2 dPx = (uv - cUV_global) / px;
        float r = length(dPx);
        float w = saturate(1.0 - pow(r / rpx, FalloffExp)) * k;

        // 방사 + 노이즈 혼합
        float2 dir = normalize(dPx + 1e-5);
        offset += (dir * 0.35 + noise * 0.65) * (DistortionPx * px) * w;

        glow = max(glow, w);
    }
    
    float3 color = SceneColor.Sample(Sampler, uv + offset).rgb;
    color += glow * EmissiveMul ;
    
    return float4(color, 1);     
}