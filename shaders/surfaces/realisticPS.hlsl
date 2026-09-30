// 写実寄りの表面シェーダーです。SP31 の照明教材 (RealisticCore.hlsli) を組み合わせています。
//   直接光   : 太陽(平行光)を Disney拡散 + Cook-Torrance鏡面反射 (Beckmann, Schlick, 幾何減衰)
//   間接光   : 半球ライトを環境光に、反射方向の空の色をフレネルで映り込みとして使う
//   凹凸     : 法線マップ、無ければアルベドの明るさから作るバンプ
//   縁の光   : リムライト(布や髪の逆光)
//   仕上げ   : 線形空間で計算し、トーンマップして表示用ガンマへ。遠景は空気遠近で空の色へ霞ませる
// Parameter: x=粗さ, y=金属度, z=凹凸の強さ, w=リムの強さ
// テクスチャ: t0=アルベド, t1=法線マップ(任意。Material.Dummy.x が1のとき使用)
#include "../common.hlsl"
#include "RealisticCore.hlsli"
#include "Shadow.hlsli"

Texture2D Albedo : register(t0);
Texture2D NormalMap : register(t1);
SamplerState TextureSampler : register(s0);

// 太陽・空・映り込みの強さです。ライトの色は既存シーンと共通の値を使い、ここで物理的な比率に広げます。
static const float SUN_INTENSITY = 1.7f;
static const float SKY_INTENSITY = 0.7f;
static const float REFLECTION_INTENSITY = 0.8f;
static const float EXPOSURE = 0.95f;
static const float HAZE_DENSITY = 0.011f;

// テクスチャにミップマップが無いため、1画素が多くのテクセルを覆う遠景ではちらつきます。
// 画素が覆う範囲を ddx/ddy で求め、その範囲に散らした4点を平均して代わりにぼかします。
float4 SampleFiltered(float2 uv)
{
    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
    uint width, height;
    Albedo.GetDimensions(width, height);
    float footprint = max(length(dx * float2(width, height)), length(dy * float2(width, height)));
    float2 a = dx * 0.35f;
    float2 b = dy * 0.35f;
    float4 blurred = (Albedo.SampleGrad(TextureSampler, uv + a + b, dx, dy)
        + Albedo.SampleGrad(TextureSampler, uv + a - b, dx, dy)
        + Albedo.SampleGrad(TextureSampler, uv - a + b, dx, dy)
        + Albedo.SampleGrad(TextureSampler, uv - a - b, dx, dy)) * 0.25f;
    float4 sharp = Albedo.Sample(TextureSampler, uv);
    // 近景(1画素≒1テクセル)はそのまま、遠景ほど4点平均へ寄せます。
    return lerp(sharp, blurred, saturate((footprint - 1.0f) * 0.5f));
}

float3 SurfaceNormal(in PS_IN input, float3 N)
{
    float strength = max(Parameter.z, 0.0f);
    float3 tangentNormal = float3(0.0f, 0.0f, 1.0f);
    if (Material.Dummy.x > 0.5f)
    {
        tangentNormal = NormalMap.Sample(TextureSampler, input.TexCoord).xyz * 2.0f - 1.0f;
        tangentNormal.xy *= strength;
    }
    else if (Material.TextureEnable)
    {
        // 隣り合うテクセルの明るさの差を傾きとして使います(SP31 のアルベドバンプ)。
        uint width, height;
        Albedo.GetDimensions(width, height);
        float2 texel = 1.5f / float2(width, height);
        float left = RealisticLuminance(Albedo.Sample(TextureSampler, input.TexCoord - float2(texel.x, 0.0f)).rgb);
        float right = RealisticLuminance(Albedo.Sample(TextureSampler, input.TexCoord + float2(texel.x, 0.0f)).rgb);
        float down = RealisticLuminance(Albedo.Sample(TextureSampler, input.TexCoord - float2(0.0f, texel.y)).rgb);
        float up = RealisticLuminance(Albedo.Sample(TextureSampler, input.TexCoord + float2(0.0f, texel.y)).rgb);
        tangentNormal = float3((left - right) * strength * 4.0f, (down - up) * strength * 4.0f, 1.0f);
    }
    float3 perturbed = RealisticPerturbNormal(N, input.WorldPosition, input.TexCoord, normalize(tangentNormal));
    // 凹凸なしの材質では元の法線をそのまま使います。
    return strength > 0.0f ? perturbed : N;
}

void main(in PS_IN input, out float4 output : SV_Target)
{
    float4 texel = Material.TextureEnable ? SampleFiltered(input.TexCoord) : float4(1.0f, 1.0f, 1.0f, 1.0f);
    float4 surface = texel * input.Diffuse;
    float3 albedo = RealisticToLinear(surface.rgb);
    float roughness = clamp(Parameter.x, 0.04f, 1.0f);
    float metallic = saturate(Parameter.y);

    float3 V = normalize(CameraPosition.xyz - input.WorldPosition);
    float3 N = SurfaceNormal(input, normalize(input.Normal));
    float3 L = normalize(-Light.Direction.xyz);
    float3 H = normalize(L + V);
    float NL = saturate(dot(N, L));
    float NV = max(dot(N, V), 0.0001f);
    float NH = saturate(dot(N, H));
    float VH = saturate(dot(V, H));

    float3 sun = RealisticToLinear(Light.Diffuse.rgb) * SUN_INTENSITY;
    float3 skyColor = RealisticToLinear(Light.SkyColor.rgb);
    float3 groundColor = RealisticToLinear(Light.GroundColor.rgb);
    float3 groundNormal = normalize(Light.GroundNormal.xyz);

    // --- 直接光: Disney拡散 + Cook-Torrance ---
    float3 f0 = lerp(0.04f.xxx, albedo, metallic);
    float3 fresnel = RealisticFresnel(f0, VH);
    float distribution = RealisticBeckmann(roughness, NH);
    float geometry = RealisticGeometricDamping(NH, NV, NL, VH);
    float3 specular = distribution * geometry * fresnel / max(4.0f * NL * NV, 0.0001f);
    float3 diffuseWeight = (1.0f - fresnel) * (1.0f - metallic);
    float disney = RealisticDisneyDiffuse(roughness, NL, NV, VH);
    // 太陽が遮られている所では直接光だけを消します。空からの光(間接光)は影の中にも届きます。
    float shadow = SampleShadow(input.WorldPosition, normalize(input.Normal), L, 0.0f);
    float3 direct = (diffuseWeight * albedo * disney + specular * REALISTIC_PI) * sun * NL * shadow;

    // --- 間接光: 半球ライト + 空の映り込み ---
    float3 ambientFresnel = RealisticFresnelRough(f0, NV, roughness);
    float3 hemisphere = RealisticHemisphere(N, groundNormal, skyColor, groundColor) * SKY_INTENSITY;
    float3 indirectDiffuse = (1.0f - ambientFresnel) * (1.0f - metallic) * albedo * hemisphere;
    float3 reflected = reflect(-V, N);
    float3 environment = RealisticHemisphere(reflected, groundNormal, skyColor, groundColor);
    float glossiness = (1.0f - roughness) * (1.0f - roughness);
    float3 indirectSpecular = environment * ambientFresnel * lerp(0.08f, 1.0f, glossiness) * REFLECTION_INTENSITY;

    // --- 縁の光 ---
    float3 rim = sun * albedo * RealisticRim(N, V, L, 3.0f) * Parameter.w * 0.5f * shadow;

    float3 color = direct + indirectDiffuse + indirectSpecular + rim;

    // --- 空気遠近: 遠くほど空の色へ霞む ---
    float distance = length(CameraPosition.xyz - input.WorldPosition);
    float haze = 1.0f - exp(-distance * HAZE_DENSITY);
    color = lerp(color, skyColor * SKY_INTENSITY * 0.9f, haze * 0.65f);

    output = float4(RealisticToDisplay(color * EXPOSURE), surface.a);
}
