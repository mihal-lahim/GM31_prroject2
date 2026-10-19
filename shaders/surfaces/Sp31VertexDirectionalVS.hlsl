// SP31 VertexDirectionalLightingVS の移植。頂点ごとに平行光の明るさを求めます。
#include "sp31/Sp31Common.hlsli"

void main(in VS_IN In, out PS_IN Out)
{
    matrix wvp = mul(mul(World, View), Projection);
    Out.Position = mul(In.Position, wvp);
    float4 worldNormal = normalize(mul(float4(In.Normal.xyz, 0.0f), World));
    Out.Normal = worldNormal;
    float light = saturate(-dot(Light.Direction.xyz, worldNormal.xyz));
    Out.Diffuse.rgb = light;
    Out.Diffuse.a = In.Diffuse.a;
    Out.TexCoord = In.TexCoord;
    Out.WorldPosition = mul(In.Position, World);
}
