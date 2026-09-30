// 影パス用。葉の画像のような透過テクスチャで、透明な所は影を落とさないように抜きます。
#include "../common.hlsl"

Texture2D Albedo : register(t0);
SamplerState TextureSampler : register(s0);

void main(in PS_IN input)
{
    clip(Albedo.Sample(TextureSampler, input.TexCoord).a * input.Diffuse.a - 0.45f);
}
