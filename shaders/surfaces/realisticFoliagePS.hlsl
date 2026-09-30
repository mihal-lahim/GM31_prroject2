// 木や草のビルボード用です。板ポリゴンの法線はカメラを向いていて照明に使えないため、
// 葉の塊を「上を向いた柔らかい面」とみなして、SP31 の教材を次のように組み合わせています。
//   太陽     : 光を包み込むように回り込ませ(ハーフランバート)、逆光では葉を透ける光を足す
//   環境光   : 半球ライト(空と地面の照り返し)
//   陰り     : 根元ほど暗く(茂みの内側や幹の足元に光が届きにくい)
//   仕上げ   : realisticPS と同じ線形計算・空気遠近・トーンマップで、周りの物と見た目を揃える
// Parameter は使いません。透明部分は深度も色も書きません。
#include "../common.hlsl"
#include "RealisticCore.hlsli"
#include "Shadow.hlsli"

Texture2D Albedo : register(t0);
SamplerState TextureSampler : register(s0);

static const float SUN_INTENSITY = 1.7f;
static const float SKY_INTENSITY = 0.7f;
static const float EXPOSURE = 0.95f;
static const float HAZE_DENSITY = 0.011f;

void main(in PS_IN input, out float4 output : SV_Target)
{
    float4 texel = Albedo.Sample(TextureSampler, input.TexCoord) * input.Diffuse;
    clip(texel.a - 0.45f);
    float3 albedo = RealisticToLinear(texel.rgb);

    float3 V = normalize(CameraPosition.xyz - input.WorldPosition);
    float3 L = normalize(-Light.Direction.xyz);
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 sun = RealisticToLinear(Light.Diffuse.rgb) * SUN_INTENSITY;
    float3 skyColor = RealisticToLinear(Light.SkyColor.rgb);
    float3 groundColor = RealisticToLinear(Light.GroundColor.rgb);

    // 葉の塊は上面ほど日が当たります。ハーフランバートで光を回り込ませます。
    float wrap = saturate(dot(up, L) * 0.5f + 0.5f);
    wrap *= wrap;
    // 太陽を背にして見たとき、薄い葉を透けた光で明るく見えます(リムライトの考え方)。
    float translucency = pow(saturate(dot(-V, L)), 4.0f) * 0.6f;
    // テクスチャ上端(v=0)が梢、下端(v=1)が根元です。
    float occlusion = lerp(1.0f, 0.55f, saturate(input.TexCoord.y));

    float3 hemisphere = RealisticHemisphere(up, normalize(Light.GroundNormal.xyz), skyColor, groundColor) * SKY_INTENSITY;
    // 木の影は太陽へ向けた板で描くため、カメラへ向いた見た目の板は自分の影と交差します。
    // 太陽側へ大きめに押し出してから調べ、自分の影は拾わず、ほかの木の影だけを受けます。
    float shadow = SampleShadow(input.WorldPosition, up, L, 1.2f);
    float3 color = albedo * (sun * (wrap * 0.85f + translucency) * shadow + hemisphere) * occlusion;

    float distance = length(CameraPosition.xyz - input.WorldPosition);
    float haze = 1.0f - exp(-distance * HAZE_DENSITY);
    color = lerp(color, skyColor * SKY_INTENSITY * 0.9f, haze * 0.65f);

    output = float4(RealisticToDisplay(color * EXPOSURE), 1.0f);
}
