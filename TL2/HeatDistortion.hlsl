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
    float glow = 0;
    
    [loop]
    for (uint s = 0; s < NumSpots; ++s)
    {
        // Spots: xy: uv, z: RadiusPx, w: Strength 
        
        //viewport 기준 uv
        float2 localUV = Spots[s].xy; //현재 Viewport 기준 [0, 1]
        //screen 기준 uv
        
        float2 globalUV = (ViewportRect.xy + localUV * ViewportRect.zw) * InvRes; // Viewport.zw = width , height
        
        // Spots.z: RadiusPx
        //float rpx = max(Spots[s].z, 1.0);
        float strength = Spots[s].w;   

        // uv→픽셀좌표로 변환해 반경(px)으로 감쇠
        float2 pxDist= (uv - globalUV) / px;
        float radius = length(pxDist);
        float w = saturate(1.0 - pow(radius / Spots[s].z, FalloffExp) ) * strength;

        // 방사 + 노이즈 혼합
        float2 dir = normalize(pxDist);
        offset += (dir * 0.35 + noise * 0.65) * (DistortionPx * px) * w;

        glow = max(glow, w);
    }
    
    float3 color = SceneColor.Sample(Sampler, uv + offset).rgb;
    color += glow * EmissiveMul;
    
    return float4(color, 1);     
}