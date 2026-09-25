#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 Out : SV_Target)
{
    float3 normal = normalize(In.Normal);
    float3 lightDirection = normalize(-Light.Direction.xyz);
    float diffuse = saturate(dot(normal, lightDirection));
    float3 base = g_Texture.Sample(g_SamplerState, In.TexCoord).rgb;
    float3 lit = base * In.Diffuse.rgb * (Light.Ambient.rgb + Light.Diffuse.rgb * diffuse);
    float3 viewDirection = normalize(CameraPosition.xyz - In.WorldPosition);
    float3 halfVector = normalize(lightDirection + viewDirection);
    float specular = pow(saturate(dot(normal, halfVector)), max(Material.Shininess, 8.0f));
    Out = float4(lit + Material.Specular.rgb * specular, In.Diffuse.a);
}
