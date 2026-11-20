// SlowAction の時間減速の画面演出(ShaderCatalog の post-time-slow)。
//   Parameter.x  intensity     減速の深さ(0〜1)。彩度低下・寒色・ビネット・外周の色ずれの強さ
//   Parameter.y  waveProgress  発動・解除の衝撃波の進み(0〜1、0なら無し)
//   Parameter.z  flash         ジャスト減速のフラッシュの残り(0〜1)
//   Parameter.w  waveSign      +1=発動(中心から外へ広がる)、-1=解除(外から中心へ戻る)
// 画面中央と HUD の文字が読めるよう、常に掛かる歪み(色ずれ)は外周だけ・ごく弱くしています。
#include "../common.hlsl"

Texture2D Scene : register(t0);
SamplerState SceneSampler : register(s0);

// 見た目の強さ(調整値)。Renderer::SetPostEffectParameters の extra がそのまま入ります。
// z は Renderer が書く「独自の値を渡したか」の印(1=渡した)なので、値には使いません。
// 渡されていなければ(ShaderGallery などから既定値で選んだとき)、下の標準値を使います。
cbuffer TimeSlowLook : register(b7)
{
    float Desaturate;
    float Tint;
    float HasCustomLook;
    float Vignette;
};

static const float kDefaultDesaturate = 0.65f;
static const float kDefaultTint = 0.35f;
static const float kDefaultVignette = 0.45f;

static const float kPi = 3.14159265f;
// 外周の色ずれの最大量(画面の割合)。文字がにじまない程度に抑えます。
static const float kEdgeShift = 0.0025f;
// 衝撃波の輪の太さ・歪み・最大半径(画面の高さを1とした距離)。
static const float kWaveWidth = 0.05f;
static const float kWaveDistortion = 0.02f;
static const float kWaveMaxRadius = 1.15f;

float4 SampleScene(float2 uv)
{
    // サンプラーは折り返しなので、端からはみ出した分は端の色にします。
    return Scene.Sample(SceneSampler, saturate(uv));
}

void main(in PS_IN input, out float4 output : SV_Target)
{
    const float intensity = saturate(Parameter.x);
    const float wave = saturate(Parameter.y);
    const float flash = saturate(Parameter.z);
    const float waveSign = Parameter.w < 0.0f ? -1.0f : 1.0f;
    const bool custom = HasCustomLook > 0.5f;
    const float desaturateAmount = saturate(custom ? Desaturate : kDefaultDesaturate);
    const float tintAmount = saturate(custom ? Tint : kDefaultTint);
    const float vignetteAmount = saturate(custom ? Vignette : kDefaultVignette);

    const float2 uv = input.TexCoord;
    uint width, height;
    Scene.GetDimensions(width, height);
    const float aspect = (float)width / max((float)height, 1.0f);
    const float2 fromCenter = uv - 0.5f;
    // 縦横比をそろえた中心からの距離。上下の端で 0.5、四隅で約 1.0 です。
    const float dist = length(float2(fromCenter.x * aspect, fromCenter.y));
    const float2 outward = fromCenter / max(length(fromCenter), 1e-4f);

    // 衝撃波: 発動は中心から外へ、解除は外から中心へ。始まりと終わりは弱めて、急に現れないようにします。
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
        SampleScene(baseUv + outward * shift).r,
        SampleScene(baseUv).g,
        SampleScene(baseUv - outward * shift).b);

    // 彩度を落として寒色へ寄せます。敵の赤い予兆は見落とせないので、強い赤ほど彩度を残します。
    const float luma = dot(color, float3(0.299f, 0.587f, 0.114f));
    const float redness = saturate((color.r - max(color.g, color.b)) * 3.0f);
    const float desaturate = desaturateAmount * intensity * (1.0f - 0.6f * redness);
    color = lerp(color, luma.xxx, desaturate);
    color = lerp(color, color * float3(0.78f, 0.96f, 1.18f), tintAmount * intensity);

    // 外周を暗く締めます。
    color *= 1.0f - vignetteAmount * intensity * smoothstep(0.4f, 1.1f, dist);

    // 衝撃波の輪を少し光らせます。
    color += ring * 0.12f * float3(0.6f, 0.9f, 1.0f);

    // ジャスト減速: 一瞬だけ色を反転させ、シアンに染めます(明暗が入れ替わるので、薄めても目立ちます)。
    // 元画面は HDR(1を超える値もある)なので、反転の前に0〜1へ収めます(realisticCompositePS と同じ)。
    const float3 flashColor = saturate((1.0f - saturate(color)) * float3(0.55f, 1.0f, 1.15f) + 0.08f);
    color = lerp(color, flashColor, flash * 0.85f);

    // 画面へ描くときアルファ合成が有効なままのことがあり、文字や粒の部分のアルファ(1未満)をそのまま出すと
    // 前のフレームの画面が透けます。最終の画面なので常に不透明にします。
    output = float4(saturate(color), 1.0f);
}
