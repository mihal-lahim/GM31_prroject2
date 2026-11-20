// 太陽の影 (Engine::ShadowMap)。レジスタは既存の RendererStateScope が触らない番号です。
cbuffer ShadowBuffer : register(b8)
{
    matrix LightViewProjection;
    // x=1テクセルの大きさ[m], y=深度の偏り, z=有効(1)/無効(0), w=1テクセルのUV幅
    float4 ShadowParams;
}

Texture2D<float> ShadowTexture : register(t4);
SamplerComparisonState ShadowSampler : register(s1);

// 影の縁をぼかすための16点のポアソンディスク(半径1の円の中に、互いに離して散らした点)です。
static const float2 SHADOW_POISSON[16] =
{
    float2(-0.94201624f, -0.39906216f), float2(0.94558609f, -0.76890725f),
    float2(-0.09418410f, -0.92938870f), float2(0.34495938f, 0.29387760f),
    float2(-0.91588581f, 0.45771432f), float2(-0.81544232f, -0.87912464f),
    float2(-0.38277543f, 0.27676845f), float2(0.97484398f, 0.75648379f),
    float2(0.44323325f, -0.97511554f), float2(0.53742981f, -0.47373420f),
    float2(-0.26496911f, -0.41893023f), float2(0.79197514f, 0.19090188f),
    float2(-0.24188840f, 0.99706507f), float2(-0.81409955f, 0.91437590f),
    float2(0.19984126f, 0.78641367f), float2(0.14383161f, -0.14100790f)
};

// 影の縁のぼかし半径(シャドウマップのテクセル数)です。
static const float SHADOW_SOFTNESS_TEXELS = 1.8f;

// 日なたなら1、影なら0を返します。境界は周囲16点を比べて柔らかくします(ポアソンディスクPCF)。
// 点の並びを場所ごとに回すので、格子状のギザギザが出ず、なめらかなノイズの縁になります。
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

    // ワールド位置から決まる回転角です(カメラが動いても影の縁の模様は地面に貼り付いたまま)。
    // sin を使わないハッシュ(Hoskins の hash13)なので、広い場所で座標が大きくても精度が落ちず縞になりません。
    float3 p3 = frac(worldPosition * (97.0f * 0.1031f));
    p3 += dot(p3, p3.zyx + 31.32f);
    float noise = frac((p3.x + p3.y) * p3.z);
    float angle = noise * 6.2831853f;
    float s = sin(angle);
    float c = cos(angle);
    float2x2 rotation = float2x2(c, -s, s, c);
    float radius = ShadowParams.w * SHADOW_SOFTNESS_TEXELS;

    float lit = 0.0f;
    [unroll] for (int i = 0; i < 16; ++i)
    {
        float2 offset = mul(SHADOW_POISSON[i], rotation) * radius;
        lit += ShadowTexture.SampleCmpLevelZero(ShadowSampler, uv + offset, depth);
    }
    lit /= 16.0f;

    // 範囲の端で影が急に途切れないよう、外周5%で日なたへ溶かします。
    float2 edge = saturate((abs(uv - 0.5f) - 0.45f) / 0.05f);
    lit = lerp(lit, 1.0f, max(edge.x, edge.y));
    // 影パスが無いシーン(ShadowParams.z=0)では常に日なたです。
    return ShadowParams.z > 0.5f ? lit : 1.0f;
}
