// 影パス用。葉の画像のような透過テクスチャで、透明な所は影を落とさないように抜きます。
#include "../common.hlsl"

Texture2D Albedo : register(t0);
SamplerState TextureSampler : register(s0);

void main(in PS_IN input)
{
    float alpha = Albedo.Sample(TextureSampler, input.TexCoord).a;
    // ミップで透明な画素と平均されて影の葉が痩せないよう、縮小した段ほどアルファを持ち上げます(realisticFoliagePS と同じ)。
    alpha *= 1.0f + max(Albedo.CalculateLevelOfDetail(TextureSampler, input.TexCoord), 0.0f) * 0.25f;
    clip(alpha * input.Diffuse.a - 0.45f);
}
