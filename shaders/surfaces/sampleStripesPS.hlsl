#include "PortableCommon.hlsli"
#include "SampleStripesCore.hlsli"

Texture2D Albedo : register(t0);
SamplerState TextureSampler : register(s0);

void main(in PS_IN input, out float4 output : SV_Target)
{
    float4 texel = Albedo.Sample(TextureSampler, input.TexCoord);
    output = ShadeSampleStripes(texel, input.Diffuse, input.Normal,
        input.TexCoord, Parameter, Light.Direction.xyz, Light.Diffuse.rgb,
        Light.Ambient.rgb);
}
