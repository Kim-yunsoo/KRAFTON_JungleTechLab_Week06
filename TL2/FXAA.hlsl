// ============================================================================
// Unreal-like FXAA (HLSL version, using your FXAAParams buffer)
// Based on NVIDIA FXAA 3.11 logic
// CORRECTED VERSION (keeps your engine's fullscreen tri + ViewportRect)
// ============================================================================

// PS b0 : 새 파라미터 패킹
cbuffer FXAAParams : register(b0)
{
    float2 InvResolution;   // 1.0 / resolution
    float FXAASpanMax;      // Max search span
    float FXAAReduceMul;    // Reduce multiplier
    // --- 16 byte boundary ---
    float FXAAReduceMin;    // Min reduce value
    int   Enabled;          // FXAA toggle
    float2 Padding;         // Padding
};

// Optional viewport input: x=StartX, y=StartY, z=SizeX, w=SizeY (pixels)
cbuffer FXAAViewportCB : register(b6)
{
    float4 ViewportRect;
}

// Match resource naming (t0/s0)
Texture2D SceneColor : register(t0);
SamplerState SceneSampler : register(s0);

struct VS_OUT
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

// Fullscreen triangle (empty input layout)
VS_OUT VS_FullScreen(uint id : SV_VertexID)
{
    float2 verts[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0)
    };

    float2 p = verts[id];
    VS_OUT o;
    o.pos = float4(p, 0.0f, 1.0f);
    o.uv = 0.5f * float2(p.x, -p.y) + 0.5f;
    return o;
}

//------------------------------------------------------------------------------
// Utility: luminance calculation (gamma-space Y' approximation)
//------------------------------------------------------------------------------
float Luma(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

//------------------------------------------------------------------------------
// Pixel Shader: Corrected Unreal-style FXAA (with viewport support)
//------------------------------------------------------------------------------
float4 PS_FXAA(VS_OUT input) : SV_Target
{
    //픽셀 좌표계를 viewport rect를 사용해서 viewportLoalPos로 변환
    float2 viewportLocalPos = input.pos.xy - ViewportRect.xy; // 0,0 ~ v
    float2 viewportUV = viewportLocalPos / max(ViewportRect.zw, float2(1e-6, 1e-6)); // viewport에 맞는 UV zw는 width height
    float2 tex = (ViewportRect.xy + viewportLocalPos) * InvResolution;

    // Bypass if disabled
    if (!Enabled)
    {
        return float4(SceneColor.Sample(SceneSampler, tex).rgb, 1.0f);
    }


    // Fetch 5 samples (center + 4 diagonals)
    float2 inv = InvResolution;

    float3 rgbNW = SceneColor.Sample(SceneSampler, tex + float2(-inv.x, -inv.y)).rgb;
    float3 rgbNE = SceneColor.Sample(SceneSampler, tex + float2(inv.x, -inv.y)).rgb;
    float3 rgbSW = SceneColor.Sample(SceneSampler, tex + float2(-inv.x, inv.y)).rgb;
    float3 rgbSE = SceneColor.Sample(SceneSampler, tex + float2(inv.x, inv.y)).rgb;
    float3 rgbM = SceneColor.Sample(SceneSampler, tex).rgb;

    // Convert to luminance
    float lumaNW = Luma(rgbNW);
    float lumaNE = Luma(rgbNE);
    float lumaSW = Luma(rgbSW);
    float lumaSE = Luma(rgbSE);
    float lumaM = Luma(rgbM);

    // Neighborhood stats
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    // Early-exit for low contrast (Unreal-style)
    float threshold = max(0.0625, lumaMax * 0.125);
    if (lumaRange < threshold)
    {
        return float4(rgbM, 1.0f);
    }

    // Edge direction (diagonal emphasis)
    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    // Direction reduction by local contrast
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * FXAAReduceMul), FXAAReduceMin);

    // Normalize using smaller axis to reduce orientation bias
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    // Scale & clamp in pixel domain, then convert to UV
    float2 dirScaled = dir * rcpDirMin;
    dirScaled = clamp(dirScaled, float2(-FXAASpanMax, -FXAASpanMax), float2(FXAASpanMax, FXAASpanMax));
    dir = dirScaled * InvResolution;

    // Sample along the corrected edge direction
    float3 rgbA = 0.5f * (
        SceneColor.Sample(SceneSampler, tex + dir * (1.0 / 3.0 - 0.5)).rgb +
        SceneColor.Sample(SceneSampler, tex + dir * (2.0 / 3.0 - 0.5)).rgb
    );

    float3 rgbB = rgbA * 0.5f + 0.25f * (
        SceneColor.Sample(SceneSampler, tex + dir * -0.5).rgb +
        SceneColor.Sample(SceneSampler, tex + dir * 0.5).rgb
    );

    // Guard against over-blur: keep within local luminance band
    float lumaB = Luma(rgbB);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        rgbB = rgbA;
    }

    // Subpixel blend (fixed factor; parameterize if needed)
    const float subpix = 0.99f;
    float3 finalColor = lerp(rgbM, rgbB, subpix);

    return float4(finalColor, 1.0f);
}

// Entry points (engine expects these)
VS_OUT mainVS(uint id : SV_VertexID)
{
    return VS_FullScreen(id);
}
float4 mainPS(VS_OUT input) : SV_Target
{
    return PS_FXAA(input);
}
