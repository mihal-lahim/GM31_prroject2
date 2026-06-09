
#include "common.hlsl"


Texture2D		g_Texture : register(t0);
SamplerState	g_SamplerState : register(s0);


void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{

	if (Material.TextureEnable)
	{
		outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);

		outDiffuse *= In.Diffuse;
        clip(outDiffuse.a - 0.5f); // ★ アルファ0.5未満を切り捨て
    }
	else
	{
		outDiffuse = In.Diffuse;
	}
	
}
