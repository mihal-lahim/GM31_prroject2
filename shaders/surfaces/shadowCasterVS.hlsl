// 影パス用。太陽から見た位置へ変換するだけで、色は計算しません。
#include "../common.hlsl"
#include "Shadow.hlsli"

void main(in VS_IN In, out PS_IN Out)
{
    float4 worldPosition = mul(In.Position, World);
    Out.Position = mul(worldPosition, LightViewProjection);
    Out.WorldPosition = worldPosition.xyz;
    Out.Normal = float3(0.0f, 1.0f, 0.0f);
    Out.Diffuse = In.Diffuse;
    Out.TexCoord = In.TexCoord;
}
