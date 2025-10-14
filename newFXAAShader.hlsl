cbuffer FXAAParams : register(b0)
{
    float2 InvResolution;
    float FXAASpanMax;
    float FXAAReduceMul;
    float FXAAReduceMin;
}

Texture2D SceneColor : register(t0);
SamplerState SceneSampler : register(s0);

struct VSInput
{
    float2 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VSOutput mainVS(VSInput input)
{
    VSOutput output;
    output.Position = float4(input.Position, 0.0f, 1.0f);
    output.TexCoord = input.TexCoord;
    return output;
}

float4 mainPS(VSOutput input) : SV_Target
{
    float2 tex = input.TexCoord;

    float3 rgbNW = SceneColor.Sample(SceneSampler, tex + float2(-InvResolution.x, -InvResolution.y)).rgb;
    float3 rgbNE = SceneColor.Sample(SceneSampler, tex + float2( InvResolution.x, -InvResolution.y)).rgb;
    float3 rgbSW = SceneColor.Sample(SceneSampler, tex + float2(-InvResolution.x,  InvResolution.y)).rgb;
    float3 rgbSE = SceneColor.Sample(SceneSampler, tex + float2( InvResolution.x,  InvResolution.y)).rgb;
    float3 rgbM  = SceneColor.Sample(SceneSampler, tex).rgb;

    const float3 LumaCoeff = float3(0.299f, 0.587f, 0.114f);
    float lumaNW = dot(rgbNW, LumaCoeff);
    float lumaNE = dot(rgbNE, LumaCoeff);
    float lumaSW = dot(rgbSW, LumaCoeff);
    float lumaSE = dot(rgbSE, LumaCoeff);
    float lumaM  = dot(rgbM,  LumaCoeff);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * FXAAReduceMul * 0.25f, FXAAReduceMin);
    float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    float2 dirScaled = dir * rcpDirMin;
    dirScaled = clamp(dirScaled, float2(-FXAASpanMax, -FXAASpanMax), float2(FXAASpanMax, FXAASpanMax));
    dir = dirScaled * InvResolution;

    float3 rgbA = SceneColor.Sample(SceneSampler, tex + dir * (1.0f / 3.0f - 0.5f)).rgb;
    float3 rgbB = SceneColor.Sample(SceneSampler, tex + dir * (2.0f / 3.0f - 0.5f)).rgb;

    float3 averaged = 0.5f * (rgbA + rgbB);
    float3 result = lerp(rgbM, averaged, 0.9f);
    float lumaResult = dot(result, LumaCoeff);

    if (lumaResult < lumaMin || lumaResult > lumaMax)
    {
        result = rgbM;
    }

    float alpha = SceneColor.Sample(SceneSampler, tex).a;
    return float4(result, alpha);
}