// 太陽の影 (Engine::ShadowMap)。レジスタは既存の RendererStateScope が触らない番号です。
cbuffer ShadowBuffer : register(b8)
{
    matrix LightViewProjection;
    // x=1テクセルの大きさ[m], y=深度の偏り, z=有効(1)/無効(0), w=1テクセルのUV幅
    float4 ShadowParams;
}

Texture2D<float> ShadowTexture : register(t4);
SamplerComparisonState ShadowSampler : register(s1);

// 日なたなら1、影なら0を返します。境界は周囲3x3を比べて柔らかくします(PCF)。
// normal は受け手の法線、toLight は太陽へ向かう向き、extraOffset は板の木のように
// 自分の影に入りやすい物のための追加の押し出し量[m]です。
float SampleShadow(float3 worldPosition, float3 normal, float3 toLight, float extraOffset)
{
    // 面の向きへ少し押し出してから調べると、自分の影で縞になる(シャドウアクネ)のを防げます。
    float texel = ShadowParams.x;
    float3 position = worldPosition + normal * (texel * 1.5f) + toLight * (texel * 0.5f + extraOffset);
    float4 projected = mul(float4(position, 1.0f), LightViewProjection);
    float2 uv = projected.xy * float2(0.5f, -0.5f) + 0.5f;
    float depth = projected.z - ShadowParams.y;

    float lit = 0.0f;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            lit += ShadowTexture.SampleCmpLevelZero(ShadowSampler, uv + float2(x, y) * ShadowParams.w, depth);
        }
    }
    lit /= 9.0f;

    // 範囲の端で影が急に途切れないよう、外周5%で日なたへ溶かします。
    float2 edge = saturate((abs(uv - 0.5f) - 0.45f) / 0.05f);
    lit = lerp(lit, 1.0f, max(edge.x, edge.y));
    // 影パスが無いシーン(ShadowParams.z=0)では常に日なたです。
    return ShadowParams.z > 0.5f ? lit : 1.0f;
}
