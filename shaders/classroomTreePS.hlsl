#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
	float4 color = g_Texture.Sample(g_SamplerState, In.TexCoord) * In.Diffuse;
	// 透明部分は深度も色も書かない。黒い矩形や縁を残さない。
	clip(color.a - 0.45f);
	outDiffuse = float4(color.rgb, 1.0f);
}
