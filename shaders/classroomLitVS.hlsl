#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    matrix wvp = mul(mul(World, View), Projection);
    Out.Position = mul(In.Position, wvp);
    Out.WorldPosition = mul(In.Position, World).xyz;
    Out.Normal = normalize(mul(float4(In.Normal.xyz, 0.0f), World).xyz);
    Out.Diffuse = In.Diffuse * Material.Diffuse;
    Out.TexCoord = In.TexCoord;
}
