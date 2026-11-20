// SP31 shader/Toon2PS.hlsl の移植(内容は同一)。
#include "sp31/Sp31Common.hlsli"

Texture2D g_Texture : register(t0);
Texture2D g_TextureRamp : register(t1);
SamplerState g_SampleState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float3 toLight = Light.Position.xyz - In.WorldPosition.xyz;
    float distanceToLight = length(toLight);
    toLight = normalize(toLight);
    float ofs = saturate(1.0f - distanceToLight / Light.PointLightparam.x);
    float3 normal = normalize(In.Normal.xyz);
    float light = 0.5f + 0.5f * dot(normal, toLight);
    light = clamp(light, 0.01f, 0.99f);
    float texv = clamp(Parameter.x, 0.01f, 0.99f);
    // ランプは参照表なので、ミップ(帯を平均した色)を使わず常に元の解像度で読みます。
    float4 toon = g_TextureRamp.SampleLevel(g_SampleState, float2(light, texv), 0.0f);
    toon *= ofs;

    outDiffuse = g_Texture.Sample(g_SampleState, In.TexCoord);
    outDiffuse.rgb *= toon.rgb * In.Diffuse.rgb * Light.Diffuse.rgb + Light.Ambient.rgb;
    outDiffuse.a *= In.Diffuse.a;

    float3 eyev = normalize(In.WorldPosition.xyz - CameraPosition.xyz);
    if (dot(normal, eyev) > -Parameter.y)
        outDiffuse.rgb *= Parameter.z;
}
