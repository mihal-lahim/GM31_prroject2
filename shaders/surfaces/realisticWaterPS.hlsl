// 写実寄りの水面です。SP31 の照明教材 (RealisticCore.hlsli) を水向けに組み合わせています。
//   映り込み : 反射方向の空の色(半球ライト)を、Schlickのフレネルで角度に応じて混ぜる
//   きらめき : 太陽の Cook-Torrance 鏡面反射(Beckmann, 粗さ小)
//   水の色   : 見下ろすほど水中の色が透け、斜めから見るほど鏡のように空を映す
//   さざ波   : 頂点の波に加え、時間で流れる正弦波の傾きで細かな法線を作る
// Parameter: x=経過時間(秒), y=さざ波の強さ, z=水面の粗さ, w=未使用
#include "../common.hlsl"
#include "RealisticCore.hlsli"
#include "Shadow.hlsli"

Texture2D Albedo : register(t0);
SamplerState TextureSampler : register(s0);

static const float SUN_INTENSITY = 1.7f;
static const float SKY_INTENSITY = 0.7f;
static const float EXPOSURE = 0.95f;
static const float HAZE_DENSITY = 0.011f;
static const float WATER_F0 = 0.02f; // 水の垂直入射の反射率

// 向きと速さの違う正弦波を重ね、その傾き(高さの微分)を返します。
float2 RippleSlope(float2 p, float time)
{
    float2 slope = 0.0f;
    const float2 directions[4] = { float2(1.0f, 0.3f), float2(-0.4f, 1.0f), float2(0.7f, -0.8f), float2(-1.0f, -0.2f) };
    const float frequencies[4] = { 2.1f, 3.3f, 5.2f, 7.9f };
    const float speeds[4] = { 0.9f, 1.3f, 1.7f, 2.3f };
    const float amplitudes[4] = { 0.050f, 0.032f, 0.020f, 0.012f };
    [unroll] for (int i = 0; i < 4; ++i)
    {
        float2 d = normalize(directions[i]);
        float phase = dot(d, p) * frequencies[i] + time * speeds[i];
        slope += d * (cos(phase) * frequencies[i] * amplitudes[i]);
    }
    return slope;
}

void main(in PS_IN input, out float4 output : SV_Target)
{
    float time = Parameter.x;
    float rippleStrength = Parameter.y > 0.0f ? Parameter.y : 1.0f;
    float roughness = clamp(Parameter.z > 0.0f ? Parameter.z : 0.08f, 0.04f, 1.0f);

    float2 slope = RippleSlope(input.WorldPosition.xz, time) * rippleStrength;
    float3 N = normalize(normalize(input.Normal) + float3(-slope.x, 0.0f, -slope.y));
    float3 V = normalize(CameraPosition.xyz - input.WorldPosition);
    float3 L = normalize(-Light.Direction.xyz);
    float3 H = normalize(L + V);
    float NL = saturate(dot(N, L));
    float NV = max(dot(N, V), 0.0001f);
    float NH = saturate(dot(N, H));
    float VH = saturate(dot(V, H));

    float3 sun = RealisticToLinear(Light.Diffuse.rgb) * SUN_INTENSITY;
    float3 skyColor = RealisticToLinear(Light.SkyColor.rgb) * SKY_INTENSITY;
    float3 groundColor = RealisticToLinear(Light.GroundColor.rgb) * SKY_INTENSITY;
    float3 groundNormal = normalize(Light.GroundNormal.xyz);

    // 空の映り込み。水平線付近ほど明るい空になるよう、反射方向の高さで少し持ち上げます。
    float3 reflected = reflect(-V, N);
    float3 environment = RealisticHemisphere(reflected, groundNormal, skyColor, groundColor)
        * lerp(1.35f, 1.0f, saturate(reflected.y));
    float fresnel = RealisticFresnel(WATER_F0.xxx, NV).x;

    // 水の中の色。テクスチャは色むらとして薄く使い、深い青緑を基本にします。
    float3 texel = RealisticToLinear(Albedo.Sample(TextureSampler, input.TexCoord + slope * 0.5f).rgb);
    float3 body = lerp(float3(0.010f, 0.045f, 0.055f), texel * float3(0.10f, 0.22f, 0.24f), 0.35f);
    float3 hemisphere = RealisticHemisphere(float3(0.0f, 1.0f, 0.0f), groundNormal, skyColor, groundColor);
    // 影の中の水面では、水中に差し込む日光ときらめきが消えます。
    float shadow = SampleShadow(input.WorldPosition, float3(0.0f, 1.0f, 0.0f), L, 0.0f);
    float3 scattered = body * (hemisphere + sun * saturate(L.y) * shadow);

    // 太陽のきらめき(Cook-Torrance)。
    float distribution = RealisticBeckmann(roughness, NH);
    float geometry = RealisticGeometricDamping(NH, NV, NL, VH);
    float3 glint = distribution * geometry * RealisticFresnel(WATER_F0.xxx, VH)
        / max(4.0f * NL * NV, 0.0001f) * sun * NL * REALISTIC_PI * shadow;

    float3 color = lerp(scattered, environment, fresnel) + glint;

    float distance = length(CameraPosition.xyz - input.WorldPosition);
    float haze = 1.0f - exp(-distance * HAZE_DENSITY);
    color = lerp(color, skyColor * 0.9f, haze * 0.65f);

    // 見下ろすと水底が透け、斜めから見るほど不透明(鏡)になります。
    // 深さによる色の沈みは深度を読めないため、真上でも少し濁る程度の不透明度で代わりにします。
    float alpha = saturate(lerp(0.74f, 1.0f, fresnel) * input.Diffuse.a / 0.78f);
    output = float4(RealisticToDisplay(color * EXPOSURE), alpha);
}
