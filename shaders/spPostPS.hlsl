#include "common.hlsl"

Texture2D Scene : register(t0);
SamplerState SceneSampler : register(s0);
cbuffer PostOptions : register(b7) { float4 ExtraOptions; };

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
        bool insideCircle = length(centered) < (ExtraOptions.z > 0.5f ? max(Parameter.z, 0.05f) : 0.3f);
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
        float noise = frac(sin(dot(input.Position.xy + Parameter.w, float2(12.9898f, 78.233f))) * 43758.5453f) - 0.5f;
        float strength = ExtraOptions.z > 0.5f ? Parameter.y : 0.25f;
        float contrast = ExtraOptions.z > 0.5f ? Parameter.z : 1.7f;
        float gray = dot(saturate(color + noise * strength), float3(0.299f, 0.587f, 0.114f));
        output = float4(pow(saturate(gray), max(contrast, 0.1f)).xxx, 1.0f);
    }
    else if (mode == 6) // mip level preview
        output = Scene.SampleLevel(SceneSampler, uv, ExtraOptions.z > 0.5f ? Parameter.y : 2.0f);
    else if (mode == 7) // shape mask
    {
        float2 centered = (uv - 0.5f) * float2((float)width / height, 1.0f);
        float radius = ExtraOptions.z > 0.5f ? max(Parameter.y, 0.05f) : 0.3f;
        float distanceFromCenter = length(centered);
        float mask = ExtraOptions.x < 0.5f ? 1.0f - smoothstep(radius - 0.02f, radius, distanceFromCenter)
            : ExtraOptions.x < 1.5f ? smoothstep(radius * 0.45f, radius * 0.55f, distanceFromCenter) * (1.0f - smoothstep(radius - 0.02f, radius, distanceFromCenter))
            : ExtraOptions.x < 2.5f ? 1.0f - smoothstep(radius - 0.02f, radius, abs(centered.x) + abs(centered.y))
            : 1.0f - smoothstep(radius - 0.02f, radius, length(centered - float2(0.0f, 0.12f)) - 0.12f * abs(centered.x));
        output = float4(mask.xxx, 1.0f);
    }
    else if (mode == 8) // RGB channel shift
    {
        float2 offset = ExtraOptions.z > 0.5f ? float2(Parameter.y, Parameter.z) : float2(0.007f, 0.004f);
        float red = Scene.Sample(SceneSampler, uv + offset).r;
        float green = Scene.Sample(SceneSampler, uv + float2(0.0f, offset.y)).g;
        float blue = Scene.Sample(SceneSampler, uv - offset).b;
        output = float4(red, green, blue, 1.0f);
    }
    else if (mode == 9) // posterization
    {
        output = Scene.Sample(SceneSampler, uv);
        float levels = ExtraOptions.z > 0.5f ? max(Parameter.y, 2.0f) : 6.0f;
        float3 color = saturate(output.rgb * (ExtraOptions.z > 0.5f ? max(ExtraOptions.x, 0.1f) : 1.0f));
        float dither = (frac(sin(dot(input.Position.xy, float2(127.1f, 311.7f))) * 43758.5453f) - 0.5f) * (ExtraOptions.z > 0.5f ? Parameter.z : 0.0f) / levels;
        color = round(saturate(color + dither) * (levels - 1.0f)) / (levels - 1.0f);
        if (ExtraOptions.z > 0.5f && Parameter.w > 0.5f) color = dot(color, float3(0.299f, 0.587f, 0.114f)).xxx;
        output.rgb = color;
    }
    else // grayscale
    {
        output = Scene.Sample(SceneSampler, uv);
        float gray = dot(output.rgb, float3(0.299f, 0.587f, 0.114f));
        output.rgb = gray.xxx;
    }
}
