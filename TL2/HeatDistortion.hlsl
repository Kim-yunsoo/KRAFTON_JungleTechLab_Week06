struct HeatSpot
{
    float2 UV;
    float RadiusPx;
    float Strength;

    float4 Color;
};

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
    HeatSpot Spots[8];
};

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
    //viewport setting
    float2 viewportLocalPos = input.pos.xy - ViewportRect.xy; 
    float2 uv = (input.pos.xy) * InvRes; // uv: RTV에 있는 viewport를 알맞게 sampling 해올 수 있다.
    float2 px = InvRes; // px == InvRes
    
    float2 timeUV = input.uv + float2(Time * 1.2, Time * 1.5);
    float2 noise = NoiseTex.Sample(Sampler, timeUV); // [0 , 1]
    noise = noise * 2 - 1; // [-1, 1] //나이스한 sampling을 위해서
    
    float2 offset = 0;
    float glowEffect = 0;
    float3 glowColor = float3(0, 0, 0);

    [loop]
    for (uint s = 0; s < NumSpots; ++s)
    {
        // Spots: xy: uv, z: RadiusPx, w: Strength 
        
        //viewport 기준 uv, spots center의 uv 좌표
        float2 localUV = Spots[s].UV; //현재 Viewport 기준 [0, 1]
        //screen 기준 uv ,spots center의 uv좌표
        float2 globalUV = (ViewportRect.xy + localUV * ViewportRect.zw) * InvRes; // Viewport.zw = width , height
        
        // Spots.z: RadiusPx
        //float rpx = max(Spots[s].z, 1.0);
        float strength = Spots[s].Strength;   

        // uv→픽셀좌표로 변환해 반경(px)으로 감쇠
        float2 pxDist= (uv - globalUV) / px;
        float radius = length(pxDist); // 중심으로 부터 얼마나 떨어졌는 지
        float attenu = saturate(1.0 - pow(radius / Spots[s].RadiusPx, FalloffExp) ) * strength; // 그걸 Radius 로 나눠서 감쇠
        

        // 방사 + 노이즈 혼합
        float2 dir = normalize(pxDist);
        offset += (dir * 0.25 + noise * 0.75) * (DistortionPx * px * 1.5) * attenu ;

        glowEffect = max(glowEffect, attenu);
        glowColor = max(glowColor, Spots[s].Color.rgb * attenu);
    }
    
    float3 color = SceneColor.Sample(Sampler, uv + offset).rgb;
    color = color + (glowColor + glowEffect) * EmissiveMul; // EmissiveMul을 빼면 빛이 너무 강하다 
    
    return float4(color, 1);     
}