#include "PortableCommon.hlsli"

void main(in VS_IN input, out PS_IN output)
{
    float4 worldPosition = mul(input.Position, World);
    output.Position = mul(worldPosition, mul(View, Projection));
    output.WorldPosition = worldPosition.xyz;
    output.Normal = normalize(mul(float4(input.Normal.xyz, 0.0f), World).xyz);
    output.Diffuse = input.Diffuse * Material.Diffuse;
    output.TexCoord = input.TexCoord;
}
