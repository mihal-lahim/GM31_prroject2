#include "common.hlsl"

void main(in VS_IN input, out PS_IN output)
{
    float4 expanded = input.Position + float4(input.Normal.xyz * Parameter.x, 0.0f);
    output.Position = mul(expanded, mul(mul(World, View), Projection));
    output.WorldPosition = mul(expanded, World).xyz;
    output.Normal = input.Normal.xyz;
    output.Diffuse = input.Diffuse;
    output.TexCoord = input.TexCoord;
}
