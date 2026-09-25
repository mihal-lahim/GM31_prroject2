#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 Out : SV_Target)
{
    float3 normal = normalize(In.Normal);
    float3 lightDirection = normalize(-Light.Direction.xyz);
    float diffuse = saturate(dot(normal, lightDirection));

    // Two offset samples soften the tiled water texture and create a subtle ripple pattern.
    float2 uv = In.TexCoord + float2(Parameter.x * 0.018f, -Parameter.x * 0.011f);
    float2 ripple = float2(sin(uv.y * 19.0f + Parameter.x), cos(uv.x * 17.0f - Parameter.x * 0.7f)) * 0.012f;
    float3 waterA = g_Texture.Sample(g_SamplerState, uv + ripple).rgb;
    float3 waterB = g_Texture.Sample(g_SamplerState, uv * 1.37f - ripple * 0.7f).rgb;
    float3 water = lerp(waterA, waterB, 0.35f);
    water = lerp(float3(0.03f, 0.25f, 0.42f), water, 0.48f);

    float3 viewDirection = normalize(CameraPosition.xyz - In.WorldPosition);
    float fresnel = pow(1.0f - saturate(dot(normal, viewDirection)), 3.0f);
    float3 halfVector = normalize(lightDirection + viewDirection);
    float highlight = pow(saturate(dot(normal, halfVector)), 48.0f) * 0.45f;
    float3 color = water * (0.48f + diffuse * 0.72f) + float3(0.18f, 0.48f, 0.62f) * fresnel + highlight;
    color += float3(0.18f, 0.42f, 0.58f) * fresnel;
    Out = float4(color, In.Diffuse.a * 0.72f);
}
