// SP31 shader/GalleryClassicPS.hlsl の移植(内容は同一)。各 Sp31*PS.hlsl が入口を選びます。
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
Texture2D Albedo : register(t0);
SamplerState TextureSampler : register(s0);

float4 Surface(Input input)
{
    float4 texel = Albedo.Sample(TextureSampler, input.TexCoord);
    return texel * input.Diffuse;
}

float3 Normal(Input input) { return normalize(input.Normal.xyz); }
float3 View(Input input) { return normalize(CameraPosition.xyz - input.WorldPosition.xyz); }
float3 Direction() { return -normalize(LightDirection.xyz); }
float3 PointDirection(Input input) { return normalize(LightPosition.xyz - input.WorldPosition.xyz); }
float Attenuation(Input input)
{
    float distance = length(LightPosition.xyz - input.WorldPosition.xyz);
    return saturate(1.0f - distance / max(LightPointParam.x, 0.1f));
}
float Specular(float3 N, float3 L, float3 V)
{
    float3 H = normalize(L + V);
    return pow(saturate(dot(N, H)), 30.0f);
}

float4 Directional(Input input) : SV_Target
{
    float4 texel = Surface(input);
    float diffuse = saturate(dot(Normal(input), Direction()));
    return float4(texel.rgb * (LightAmbient.rgb + LightDiffuse.rgb * diffuse), texel.a);
}

float4 BlinnPhong(Input input) : SV_Target
{
    float4 texel = Directional(input);
    float3 N = Normal(input);
    float3 L = Direction();
    texel.rgb += LightDiffuse.rgb * Specular(N, L, View(input)) *
        step(0.0f, dot(N, L));
    return texel;
}

float4 Hemisphere(Input input) : SV_Target
{
    float4 texel = Surface(input);
    float3 N = Normal(input);
    float blend = saturate(0.5f + 0.5f * dot(N, normalize(GroundNormal.xyz)));
    float3 hemi = lerp(GroundColor.rgb, SkyColor.rgb, blend);
    float diffuse = saturate(dot(N, Direction()));
    return float4(texel.rgb * (hemi + LightDiffuse.rgb * diffuse), texel.a);
}

float4 Point(Input input) : SV_Target
{
    float4 texel = Surface(input);
    float3 N = Normal(input);
    float3 L = PointDirection(input);
    float attenuation = Attenuation(input);
    float diffuse = saturate(dot(N, L));
    float specular = Specular(N, L, View(input)) * step(0.0f, dot(N, L));
    texel.rgb = texel.rgb * (LightAmbient.rgb + LightDiffuse.rgb * diffuse * attenuation)
        + LightDiffuse.rgb * specular * attenuation;
    return texel;
}

float4 Rim(Input input) : SV_Target
{
    float4 texel = Point(input);
    float3 N = Normal(input);
    float3 V = View(input);
    float3 L = PointDirection(input);
    float rim = pow(1.0f - saturate(dot(N, V)), max(LightPointParam.y, 0.1f));
    float backlight = 0.5f * (1.0f - dot(L, V));
    texel.rgb += LightDiffuse.rgb * rim * backlight * Attenuation(input);
    return texel;
}

float4 Spot(Input input) : SV_Target
{
    float4 texel = Point(input);
    float3 fromLight = normalize(input.WorldPosition.xyz - LightPosition.xyz);
    float cone = dot(fromLight, normalize(LightDirection.xyz));
    float edge = cos(max(LightAngle.x, 0.01f));
    float spot = pow(saturate((cone - edge) / max(1.0f - edge, 0.0001f)),
        max(LightPointParam.y, 0.1f));
    float4 base = Surface(input);
    texel.rgb = base.rgb * LightAmbient.rgb +
        (texel.rgb - base.rgb * LightAmbient.rgb) * spot;
    return texel;
}
