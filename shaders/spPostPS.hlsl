#include "common.hlsl"

Texture2D Scene : register(t0);
SamplerState SceneSampler : register(s0);

void main(in PS_IN input, out float4 output : SV_Target)
{
    int mode = (int)(Parameter.x + 0.5f);
    uint width, height;
    Scene.GetDimensions(width, height);
    float2 uv = input.TexCoord;
    float block = max(Parameter.y, 1.0f);
    float2 blockStart = floor(uv * float2(width, height) / block) * block;
    if (mode == 2 || mode == 4) // mosaic / circle mosaic
    {
        float2 centered = (uv - 0.5f) * float2((float)width / height, 1.0f);
        bool insideCircle = length(centered) < 0.3f;
        output = (mode == 4 && !insideCircle)
            ? Scene.Sample(SceneSampler, uv)
            : Scene.Load(int3(min(int2(blockStart), int2(width - 1, height - 1)), 0));
    }
    else if (mode == 3) // four-tap mosaic average
    {
        int2 bounds = int2(width - 1, height - 1);
        output = 0.25f * (
            Scene.Load(int3(min(int2(blockStart + block * 0.25f), bounds), 0))
            + Scene.Load(int3(min(int2(blockStart + float2(block * 0.75f, block * 0.25f)), bounds), 0))
            + Scene.Load(int3(min(int2(blockStart + float2(block * 0.25f, block * 0.75f)), bounds), 0))
            + Scene.Load(int3(min(int2(blockStart + block * 0.75f), bounds), 0)));
    }
    else if (mode == 5) // horror: noise and contrast
    {
        float3 color = Scene.Sample(SceneSampler, uv).rgb;
        float noise = frac(sin(dot(input.Position.xy, float2(12.9898f, 78.233f))) * 43758.5453f) - 0.5f;
        float gray = dot(saturate(color + noise * 0.25f), float3(0.299f, 0.587f, 0.114f));
        output = float4(pow(saturate(gray), 1.7f).xxx, 1.0f);
    }
    else if (mode == 6) // mip level preview
        output = Scene.SampleLevel(SceneSampler, uv, 2.0f);
    else if (mode == 7) // shape mask
    {
        float2 centered = (uv - 0.5f) * float2((float)width / height, 1.0f);
        float mask = 1.0f - smoothstep(0.28f, 0.30f, length(centered));
        output = float4(mask.xxx, 1.0f);
    }
    else if (mode == 8) // RGB channel shift
    {
        float2 offset = float2(0.007f, 0.004f);
        float red = Scene.Sample(SceneSampler, uv + offset).r;
        float green = Scene.Sample(SceneSampler, uv + float2(0.0f, offset.y)).g;
        float blue = Scene.Sample(SceneSampler, uv - offset).b;
        output = float4(red, green, blue, 1.0f);
    }
    else if (mode == 9) // posterization
    {
        output = Scene.Sample(SceneSampler, uv);
        output.rgb = round(saturate(output.rgb) * 5.0f) / 5.0f;
    }
    else // grayscale
    {
        output = Scene.Sample(SceneSampler, uv);
        float gray = dot(output.rgb, float3(0.299f, 0.587f, 0.114f));
        output.rgb = gray.xxx;
    }
}
