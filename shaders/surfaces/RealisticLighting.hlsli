// 写実表面の照明をまとめたものです。realisticPS(テクスチャのある物)と realisticArenaPS(手続きの石材)が共有します。
// 先に common.hlsl・RealisticCore.hlsli・Shadow.hlsli を読み込んでから使います。
//   直接光 : 太陽(平行光)を Disney拡散 + GGX鏡面反射 (GGX, Smith高さ相関, Schlick)
//   間接光 : 半球ライトを環境光に、手続きの空(空・太陽・地面)をフレネルで映り込みとして使う
//   縁の光 : リムライト(布や髪の逆光)
//   仕上げ : 線形空間で計算し、空気遠近で霞ませ、トーンマップして表示用ガンマへ

// 太陽・空・映り込みの強さです。ライトの色は既存シーンと共通の値を使い、ここで物理的な比率に広げます。
static const float SUN_INTENSITY = 1.7f;
static const float SKY_INTENSITY = 0.7f;
static const float REFLECTION_INTENSITY = 0.8f;
static const float EXPOSURE = 0.95f;
static const float HAZE_DENSITY = 0.011f;

struct RealisticSurface
{
    float3 Albedo;          // 線形の色
    float Roughness;
    float Metallic;
    float3 Normal;          // 凹凸を含む法線
    float3 GeometryNormal;  // 面そのものの法線(影の押し出しに使う)
    float Occlusion;        // 環境光の遮られ方(1=遮られない、0=真っ暗)。目地や接地の陰に使う
    float RimStrength;
};

float3 RealisticShade(RealisticSurface surface, float3 worldPosition)
{
    float3 albedo = surface.Albedo;
    float roughness = clamp(surface.Roughness, 0.04f, 1.0f);
    float metallic = saturate(surface.Metallic);
    float3 N = surface.Normal;

    float3 V = normalize(CameraPosition.xyz - worldPosition);
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

    // --- 直接光: Disney拡散 + GGX ---
    float3 f0 = lerp(0.04f.xxx, albedo, metallic);
    float3 fresnel = RealisticFresnel(f0, VH);
    float distribution = RealisticGGX(roughness, NH);
    float geometry = RealisticSmithG(roughness, NV, NL);
    float3 specular = distribution * geometry * fresnel / max(4.0f * NL * NV, 0.0001f);
    float3 diffuseWeight = (1.0f - fresnel) * (1.0f - metallic);
    float disney = RealisticDisneyDiffuse(roughness, NL, NV, VH);
    // 太陽が遮られている所では直接光だけを消します。空からの光(間接光)は影の中にも届きます。
    float shadow = SampleShadow(worldPosition, surface.GeometryNormal, L, 0.0f);
    float3 direct = (diffuseWeight * albedo * disney + specular * REALISTIC_PI) * sun * NL * shadow;

    // --- 間接光: 半球ライト + 手続きの空の映り込み ---
    float occlusion = saturate(surface.Occlusion);
    float3 ambientFresnel = RealisticFresnelRough(f0, NV, roughness);
    float3 hemisphere = RealisticHemisphere(N, groundNormal, skyColor, groundColor) * SKY_INTENSITY;
    float3 indirectDiffuse = (1.0f - ambientFresnel) * (1.0f - metallic) * albedo * hemisphere * occlusion;
    float3 reflected = reflect(-V, N);
    // 影の中では、太陽の映り込みを弱めます(空の光は残ります)。
    float3 environment = RealisticSkyEnvironment(reflected, groundNormal, skyColor, groundColor,
        L, sun, roughness, lerp(0.15f, 1.0f, shadow));
    float glossiness = (1.0f - roughness) * (1.0f - roughness);
    // 映り込みの遮られ方は、拡散より弱く効かせます(凹みの奥でも滑らかな縁は光るため)。
    float specularOcclusion = lerp(occlusion, 1.0f, 0.4f);
    float3 indirectSpecular = environment * ambientFresnel * lerp(0.08f, 1.0f, glossiness)
        * REFLECTION_INTENSITY * specularOcclusion;

    // --- 縁の光 ---
    float3 rim = sun * albedo * RealisticRim(N, V, L, 3.0f) * surface.RimStrength * 0.5f * shadow;

    return direct + indirectDiffuse + indirectSpecular + rim;
}

// 空気遠近(遠くほど空の色へ霞む)とトーンマップをかけ、表示用の色にします。
float4 RealisticFinish(float3 color, float3 worldPosition, float alpha)
{
    float3 skyColor = RealisticToLinear(Light.SkyColor.rgb);
    float distance = length(CameraPosition.xyz - worldPosition);
    float haze = 1.0f - exp(-distance * HAZE_DENSITY);
    color = lerp(color, skyColor * SKY_INTENSITY * 0.9f, haze * 0.65f);
    return float4(RealisticToDisplay(color * EXPOSURE), alpha);
}
