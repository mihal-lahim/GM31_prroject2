// 写実寄りの表面シェーダーです。照明は RealisticLighting.hlsli(SP31 の照明教材を組み合わせたもの)にあります。
//   直接光   : 太陽(平行光)を Disney拡散 + GGX鏡面反射 (GGX, Smith高さ相関, Schlick)
//   間接光   : 半球ライトを環境光に、手続きの空(大気の霞・太陽・地面の照り返し)をフレネルで映り込みとして使う
//   凹凸     : 法線マップ、無ければアルベドの明るさから作るバンプ
//   縁の光   : リムライト(布や髪の逆光)
//   仕上げ   : 線形空間で計算し、トーンマップして表示用ガンマへ。遠景は空気遠近で空の色へ霞ませる
// Parameter: x=粗さ, y=金属度, z=凹凸の強さ, w=リムの強さ
// テクスチャ: t0=アルベド, t1=法線マップ(任意。Material.Dummy.x が1のとき使用)
#include "../common.hlsl"
#include "RealisticCore.hlsli"
#include "Shadow.hlsli"
#include "RealisticLighting.hlsli"

Texture2D Albedo : register(t0);
Texture2D NormalMap : register(t1);
SamplerState TextureSampler : register(s0);

// テクスチャはふつうミップ付きで読み込むので、そのまま標本化すれば遠景もちらつきません。
// ミップが1段しか無いテクスチャ(手続き生成など)だけは、画素が覆う範囲を ddx/ddy で求め、
// その範囲に散らした4点を平均して代わりにぼかします。
float4 SampleFiltered(float2 uv)
{
    uint width, height, levels;
    Albedo.GetDimensions(0, width, height, levels);
    float4 sharp = Albedo.Sample(TextureSampler, uv);
    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
    float footprint = max(length(dx * float2(width, height)), length(dy * float2(width, height)));
    float2 a = dx * 0.35f;
    float2 b = dy * 0.35f;
    float4 blurred = (Albedo.SampleGrad(TextureSampler, uv + a + b, dx, dy)
        + Albedo.SampleGrad(TextureSampler, uv + a - b, dx, dy)
        + Albedo.SampleGrad(TextureSampler, uv - a + b, dx, dy)
        + Albedo.SampleGrad(TextureSampler, uv - a - b, dx, dy)) * 0.25f;
    // 近景(1画素≒1テクセル)はそのまま、遠景ほど4点平均へ寄せます。ミップがあれば常にそのままです。
    float blend = levels > 1 ? 0.0f : saturate((footprint - 1.0f) * 0.5f);
    return lerp(sharp, blurred, blend);
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
    float3 perturbed = RealisticPerturbNormal(N, input.WorldPosition, input.TexCoord,
        RealisticSafeNormalize(tangentNormal, float3(0.0f, 0.0f, 1.0f)));
    // 凹凸なしの材質では元の法線をそのまま使います。
    return strength > 0.0f ? perturbed : N;
}

void main(in PS_IN input, out float4 output : SV_Target)
{
    float4 texel = Material.TextureEnable ? SampleFiltered(input.TexCoord) : float4(1.0f, 1.0f, 1.0f, 1.0f);
    float4 color = texel * input.Diffuse;

    RealisticSurface surface;
    surface.Albedo = RealisticToLinear(color.rgb);
    surface.Roughness = Parameter.x;
    surface.Metallic = Parameter.y;
    surface.GeometryNormal = RealisticSafeNormalize(input.Normal, float3(0.0f, 1.0f, 0.0f));
    surface.Normal = SurfaceNormal(input, surface.GeometryNormal);
    surface.Occlusion = 1.0f;
    surface.RimStrength = Parameter.w;

    output = RealisticFinish(RealisticShade(surface, input.WorldPosition), input.WorldPosition, color.a);
}
