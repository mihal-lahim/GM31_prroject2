// SP31 shader/Toon1PS.hlsl の移植(内容は同一)。
#include "sp31/Sp31Common.hlsli"

Texture2D g_Texture : register(t0);
SamplerState g_SampleState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float3 toLight = Light.Position.xyz - In.WorldPosition.xyz;
    float distanceToLight = length(toLight);
    toLight = normalize(toLight);
    float ofs = saturate(1.0f - distanceToLight / Light.PointLightparam.x);
    float3 normal = normalize(In.Normal.xyz);
    float light = 0.5f + 0.5f * dot(normal, toLight);

    if (light < Parameter.x)
        light = 114.0f / 255.0f;
    else if (light < Parameter.y)
        light = 195.0f / 255.0f;
    else
        light = 1.0f;
    light *= ofs;

    outDiffuse = g_Texture.Sample(g_SampleState, In.TexCoord);
    outDiffuse.rgb *= In.Diffuse.rgb * Light.Diffuse.rgb * light + Light.Ambient.rgb;
    outDiffuse.a *= In.Diffuse.a;

    float3 eyev = normalize(In.WorldPosition.xyz - CameraPosition.xyz);
    if (dot(normal, eyev) > -Parameter.z)
        outDiffuse.rgb *= Parameter.w;
}
