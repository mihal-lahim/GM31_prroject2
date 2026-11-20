// 写実ポストの最後のパス: 元画面に接地の陰とブルームを合わせ、時間減速の演出とふだんのビネットを掛けて画面へ描きます。
//   Parameter.x = aoStrength      接地の陰の強さ(0で無し)
//   Parameter.z = bloomIntensity  ブルームの強さ(0で無し)
//   t2 = 接地の陰(1/2)、t4 = 中くらいのブルーム(1/4)、t6 = 広いブルーム(1/8)
//   b9 = 時間減速(PostOptions: intensity, waveProgress, flash, waveSign / PostExtra: desaturate, tint, 独自か, vignette)
//        と PostLook.x = ふだんのビネット。時間減速の見た目は post-time-slow(timeSlowPS.hlsl)と同じです。
#include "postRealisticCommon.hlsli"

static const float kDefaultDesaturate = 0.65f;
static const float kDefaultTint = 0.35f;
static const float kDefaultVignette = 0.45f;
static const float kDefaultBaseVignette = 0.14f;

static const float kPi = 3.14159265f;
static const float kEdgeShift = 0.0025f;
static const float kWaveWidth = 0.05f;
static const float kWaveDistortion = 0.02f;
static const float kWaveMaxRadius = 1.15f;

// 明るさの肩: 1付近から上をなめらかに丸め、超えた分は少し白へ寄せて「光っている」ように見せます。
// 0.92 より暗い色(ふつうの表面)は変えません。
static const float kShoulderStart = 0.92f;

float3 SampleSceneColor(float2 uv)
{
    return SampleClamped(Scene, saturate(uv), 0.0f);
}

float3 CompressHighlights(float3 color)
{
    float peak = max(color.r, max(color.g, color.b));
    float3 result = color;
    if (peak > kShoulderStart)
    {
        float over = peak - kShoulderStart;
        float width = 1.0f - kShoulderStart;
        float compressed = kShoulderStart + width * (over / (over + width));
        result = color * (compressed / peak);
        // 大きく超えた光ほど白へ寄せます(色は保ったまま芯が白く光る)。
        result = lerp(result, compressed.xxx, saturate((peak - 1.0f) * 0.25f) * 0.5f);
    }
    return result;
}

void main(in PS_IN input, out float4 output : SV_Target)
{
    const float intensity = saturate(PostOptions.x);
    const float wave = saturate(PostOptions.y);
    const float flash = saturate(PostOptions.z);
    const float waveSign = PostOptions.w < 0.0f ? -1.0f : 1.0f;
    const bool custom = PostExtra.z > 0.5f;
    const float desaturateAmount = saturate(custom ? PostExtra.x : kDefaultDesaturate);
    const float tintAmount = saturate(custom ? PostExtra.y : kDefaultTint);
    const float vignetteAmount = saturate(custom ? PostExtra.w : kDefaultVignette);
    const float baseVignette = saturate(custom ? PostLook.x : kDefaultBaseVignette);

    const float2 uv = input.TexCoord;
    uint width, height, levels;
    Scene.GetDimensions(0, width, height, levels);
    const float aspect = (float)width / max((float)height, 1.0f);
    const float2 fromCenter = uv - 0.5f;
    const float dist = length(float2(fromCenter.x * aspect, fromCenter.y));
    const float2 outward = fromCenter / max(length(fromCenter), 1e-4f);

    // --- 時間減速の衝撃波(発動は中心から外へ、解除は外から中心へ)と外周の色ずれ ---
    float ring = 0.0f;
    if (wave > 0.0f && wave < 1.0f)
    {
        const float radius = (waveSign > 0.0f ? wave : 1.0f - wave) * kWaveMaxRadius;
        ring = (1.0f - smoothstep(0.0f, kWaveWidth, abs(dist - radius))) * sin(wave * kPi);
    }
    const float edge = smoothstep(0.55f, 1.05f, dist);
    const float2 baseUv = uv - outward * (ring * kWaveDistortion * waveSign);
    const float shift = kEdgeShift * edge * intensity + ring * 0.006f;
    float3 color = float3(
        SampleSceneColor(baseUv + outward * shift).r,
        SampleSceneColor(baseUv).g,
        SampleSceneColor(baseUv - outward * shift).b);

    // --- 接地の陰: 1を超えて光っている所(剣閃・火花)には掛けず、光を濁らせません ---
    const float ao = SampleClamped(WorkHalf, baseUv, 0.0f).r;
    float aoFactor = lerp(1.0f, ao, saturate(Parameter.x));
    const float peak = max(color.r, max(color.g, color.b));
    aoFactor = lerp(aoFactor, 1.0f, saturate(peak - 1.0f));
    color *= aoFactor;

    // --- ブルーム(中くらい0.65 + 広い0.35)を足し、明るすぎる所だけ肩で丸めます ---
    const float3 bloom = SampleClamped(WorkQuarter, baseUv, 0.0f) * 0.65f + SampleClamped(WorkEighth, baseUv, 0.0f) * 0.35f;
    color += bloom * max(Parameter.z, 0.0f);
    color = CompressHighlights(color);

    // --- 時間減速: 彩度を落として寒色へ(強い赤は残す)、外周を暗く ---
    const float luma = dot(color, float3(0.299f, 0.587f, 0.114f));
    const float redness = saturate((color.r - max(color.g, color.b)) * 3.0f);
    const float desaturate = desaturateAmount * intensity * (1.0f - 0.6f * redness);
    color = lerp(color, luma.xxx, desaturate);
    color = lerp(color, color * float3(0.78f, 0.96f, 1.18f), tintAmount * intensity);
    color *= 1.0f - vignetteAmount * intensity * smoothstep(0.4f, 1.1f, dist);

    // --- ふだんの控えめなビネット(周辺を少しだけ締める) ---
    color *= 1.0f - baseVignette * smoothstep(0.45f, 1.15f, dist);

    // --- 衝撃波の輪の光と、ジャスト減速のフラッシュ ---
    color += ring * 0.12f * float3(0.6f, 0.9f, 1.0f);
    const float3 flashColor = saturate((1.0f - saturate(color)) * float3(0.55f, 1.0f, 1.15f) + 0.08f);
    color = lerp(color, flashColor, flash * 0.85f);

    output = float4(saturate(color), 1.0f);
}
