#include "common.hlsl"

void main(in VS_IN input, out PS_IN output)
{
    output.Position = mul(input.Position, mul(mul(World, View), Projection));
    output.WorldPosition = mul(input.Position, World).xyz;
    output.Normal = normalize(mul(float4(input.Normal.xyz, 0.0f), World).xyz);
    float3 L = normalize(-Light.Direction.xyz);
    float lighting = saturate(dot(output.Normal, L));
    output.Diffuse = input.Diffuse * Material.Diffuse;
    output.Diffuse.rgb *= Light.Ambient.rgb + Light.Diffuse.rgb * lighting;
    output.TexCoord = input.TexCoord;
}
