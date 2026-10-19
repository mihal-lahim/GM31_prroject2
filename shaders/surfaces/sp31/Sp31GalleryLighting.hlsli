// SP31 shader/GalleryLightingPS.hlsl の移植(内容は同一)。
struct Input
{
    float4 Position : SV_POSITION;
    float4 WorldPosition : POSITION0;
    float4 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

cbuffer LightBuffer : register(b4)
{
    int4 LightFlags;
    float4 LightDirection;
    float4 LightDiffuse;
    float4 LightAmbient;
    float4 LightPosition;
    float4 LightPointParam;
    float4 SkyColor;
    float4 GroundColor;
    float4 GroundNormal;
    float4 LightAngle;
};

cbuffer CameraBuffer : register(b5) { float4 CameraPosition; }
cbuffer ParameterBuffer : register(b6) { float4 Parameter; }
Texture2D Albedo : register(t0);
SamplerState TextureSampler : register(s0);

static const float PI = 3.14159265f;

float3 Shade(float3 albedo, float3 N, float3 V, float3 worldPosition,
    float3 lightPosition, float roughness, float metallic, bool disney)
{
    float3 Lvector = lightPosition - worldPosition;
    float distance = length(Lvector);
    float3 L = Lvector / max(distance, 0.0001f);
    float3 H = normalize(L + V);
    float NL = saturate(dot(N, L));
    float NV = saturate(dot(N, V));
    float NH = saturate(dot(N, H));
    float VH = saturate(dot(V, H));
    if (NL <= 0.0f || NV <= 0.0f) return 0.0f;

    float alpha = max(roughness * roughness, 0.0025f);
    float alpha2 = alpha * alpha;
    float denom = NH * NH * (alpha2 - 1.0f) + 1.0f;
    float distribution = alpha2 / max(PI * denom * denom, 0.0001f);
    float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
    float geometryV = NV / (NV * (1.0f - k) + k);
    float geometryL = NL / (NL * (1.0f - k) + k);
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 fresnel = F0 + (1.0f - F0) * pow(1.0f - VH, 5.0f);
    float3 specular = distribution * geometryV * geometryL * fresnel /
        max(4.0f * NL * NV, 0.0001f);

    float diffuseFactor = 1.0f;
    if (disney)
    {
        float FD90 = 0.5f + 2.0f * roughness * VH * VH;
        float FL = 1.0f + (FD90 - 1.0f) * pow(1.0f - NL, 5.0f);
        float FV = 1.0f + (FD90 - 1.0f) * pow(1.0f - NV, 5.0f);
        diffuseFactor = FL * FV;
    }
    float3 diffuse = (1.0f - metallic) * (1.0f - fresnel) *
        albedo * diffuseFactor / PI;
    float attenuation = pow(saturate(1.0f - distance / max(LightPointParam.x, 0.1f)), 2.0f);
    return (diffuse + specular) * LightDiffuse.rgb * NL * attenuation;
}

float4 Cook(Input input) : SV_Target
{
    float4 texel = Albedo.Sample(TextureSampler, input.TexCoord);
    float3 N = normalize(input.Normal.xyz);
    float3 V = normalize(CameraPosition.xyz - input.WorldPosition.xyz);
    float3 result = Shade(texel.rgb * input.Diffuse.rgb, N, V,
        input.WorldPosition.xyz, LightPosition.xyz,
        max(Parameter.x, 0.05f), saturate(Parameter.y), false);
    result += texel.rgb * LightAmbient.rgb;
    return float4(result, texel.a * input.Diffuse.a);
}

float4 Disney(Input input) : SV_Target
{
    float4 texel = Albedo.Sample(TextureSampler, input.TexCoord);
    float3 N = normalize(input.Normal.xyz);
    float3 V = normalize(CameraPosition.xyz - input.WorldPosition.xyz);
    float3 result = texel.rgb * LightAmbient.rgb;
    const int count = clamp((int)round(Parameter.z), 1, 10);
    [loop] for (int i = 0; i < count; ++i)
    {
        float angle = 2.0f * PI * i / count;
        float3 position = LightPosition.xyz + float3(cos(angle), 0.0f, sin(angle)) * 1.2f;
        result += Shade(texel.rgb * input.Diffuse.rgb, N, V,
            input.WorldPosition.xyz, position,
            max(Parameter.x, 0.05f), saturate(Parameter.y), true);
    }
    return float4(result, texel.a * input.Diffuse.a);
}
