// SP31 の照明教材を部品として取り出したものです。realisticPS.hlsl がこれらを組み合わせます。
//   GGX / Smith(高さ相関)         : 写実表面の鏡面反射(realisticPS・realisticArenaPS)
//   空の映り込み                  : 手続きで作る空・太陽・地面の環境反射(RealisticSkyEnvironment)
//   Beckmann / Schlick / 幾何減衰 : SP31 CookTorranceSub.hlsl(水面が使います)
//   Disney拡散                   : SP31 DisneyPBR (GM31 spSurfacePS mode 9 と同じ式)
//   半球ライト                    : SP31 HemiSphereLightingPS.hlsl
//   リムライト(逆光の縁)          : SP31 LimLightingPS.hlsl
//   アルベドからのバンプ          : GM31 spSurfacePS mode 12 (SP31 BumpPS の法線回転を一般化)

static const float REALISTIC_PI = 3.14159265f;

// マイクロファセットの分布関数 (SP31 CalculateBeckmann)。m は粗さです。
float RealisticBeckmann(float m, float nh)
{
    // SP31 はゼロ除算対策に早期returnしていましたが、FXCが未初期化と誤検知するため下限で抑えます。
    nh = max(nh, 0.0001f);
    float m2 = max(m * m, 0.0001f);
    float nh2 = nh * nh;
    float ex = -(1.0f - nh2) / (m2 * nh2);
    return exp(ex) / (REALISTIC_PI * m2 * nh2 * nh2);
}

// GGX (Trowbridge-Reitz) の分布関数です。roughness は見た目の粗さで、α=roughness² として使います。
// Beckmann より光の裾野が長く、金属や濡れた面のハイライトが自然ににじみます。
float RealisticGGX(float roughness, float nh)
{
    float alpha = max(roughness * roughness, 0.002f);
    float alpha2 = alpha * alpha;
    float denominator = nh * nh * (alpha2 - 1.0f) + 1.0f;
    return alpha2 / max(REALISTIC_PI * denominator * denominator, 1e-7f);
}

// Smith の幾何減衰(高さ相関あり)です。微小な凹凸が光や視線を遮る割合を、GGX と同じ α で求めます。
// 返すのは G そのもので、鏡面反射は D * G * F / (4 NL NV) で求めます。
float RealisticSmithG(float roughness, float nv, float nl)
{
    float alpha = max(roughness * roughness, 0.002f);
    float alpha2 = alpha * alpha;
    float lambdaV = nl * sqrt(nv * nv * (1.0f - alpha2) + alpha2);
    float lambdaL = nv * sqrt(nl * nl * (1.0f - alpha2) + alpha2);
    float visibility = 0.5f / max(lambdaV + lambdaL, 1e-5f);
    return saturate(visibility * 4.0f * nl * nv);
}

// フレネル項のSchlick近似 (SP31 CalculateFresnel をカラーのF0へ拡張)
float3 RealisticFresnel(float3 f0, float cosine)
{
    return f0 + (1.0f - f0) * pow(1.0f - saturate(cosine), 5.0f);
}

// 粗い面ほど縁の反射が弱まるSchlick近似です。環境光の反射に使います。
float3 RealisticFresnelRough(float3 f0, float cosine, float roughness)
{
    return f0 + (max((1.0f - roughness).xxx, f0) - f0) * pow(1.0f - saturate(cosine), 5.0f);
}

// 幾何減衰率 (SP31 CalculateGeometricDamping)
float RealisticGeometricDamping(float nh, float nv, float nl, float vh)
{
    vh = max(vh, 0.0001f);
    return saturate(min(2.0f * nh * nv / vh, 2.0f * nh * nl / vh));
}

// Disney(Burley)拡散。粗い面ほど縁が明るくなる補正です。
float RealisticDisneyDiffuse(float roughness, float nl, float nv, float vh)
{
    float fd90 = 0.5f + 2.0f * roughness * vh * vh;
    return (1.0f + (fd90 - 1.0f) * pow(1.0f - nl, 5.0f))
        * (1.0f + (fd90 - 1.0f) * pow(1.0f - nv, 5.0f));
}

// 半球ライト。法線が空を向くほど空の色、地面を向くほど地面の照り返しの色になります。
float3 RealisticHemisphere(float3 direction, float3 groundNormal, float3 skyColor, float3 groundColor)
{
    float blend = saturate(0.5f + 0.5f * dot(direction, groundNormal));
    return lerp(groundColor, skyColor, blend);
}

// 手続きで作る空の映り込み(環境マップの代わり)です。テクスチャを使わず、反射の向きから次の3つを足します。
//   ① 空: 天頂は空の色、地平線に近いほど明るく霞む(大気の散乱)。太陽の側の地平線は少し暖かく光る。
//   ② 太陽: 反射が太陽を向くと光る。粗い面ほど広く・弱く、滑らかな面ほど鋭く・強くなる。
//   ③ 地面: 下向きの反射は地面の照り返し。地平線の近くは空へなめらかにつながる。
// 粗い面ほど、上下の差も地平線の帯もぼやけて、平均の色へ近づきます(ぼかした環境マップの代わり)。
// sunVisibility は太陽の映り込みの残し方(影の中では弱める)です。色はすべて線形の値で受け取ります。
float3 RealisticSkyEnvironment(float3 direction, float3 groundNormal, float3 skyColor, float3 groundColor,
    float3 toSun, float3 sunColor, float roughness, float sunVisibility)
{
    float up = dot(direction, groundNormal);
    float sunAlong = saturate(dot(direction, toSun));
    // 太陽の方角(水平面の上で)へ向くほど、地平線の帯を暖かくします。
    float3 sunFlat = normalize(toSun - groundNormal * dot(toSun, groundNormal) + 1e-4f);
    float3 dirFlat = normalize(direction - groundNormal * up + 1e-4f);
    float towardSun = saturate(dot(dirFlat, sunFlat) * 0.5f + 0.5f);

    float3 horizon = skyColor * 1.35f + sunColor * (0.05f + 0.10f * towardSun * towardSun);
    float3 zenith = skyColor * 0.85f;
    float3 sky = lerp(horizon, zenith, pow(saturate(up), 0.45f));
    float3 ground = lerp(horizon * 0.55f, groundColor, saturate(-up * 2.5f));
    // 地平線の境目は、粗い面ほど広くぼかします。
    float edge = 0.04f + roughness * 0.55f;
    float3 environment = lerp(ground, sky, smoothstep(-edge, edge, up));

    // 太陽の映り込み。指数は粗さで2〜2048、明るさは指数に合わせて保ち、粗いほど全体を弱めます。
    float power = exp2(lerp(11.0f, 1.0f, sqrt(saturate(roughness))));
    float sunLobe = pow(sunAlong, power) * (power + 2.0f) / 8.0f;
    float sunStrength = lerp(0.35f, 0.02f, saturate(roughness));
    environment += sunColor * min(sunLobe * sunStrength, 6.0f) * sunVisibility * saturate(up * 4.0f + 0.3f);

    // 粗い面は、どの向きも空と地面の平均に近い色を映します。
    float3 average = (skyColor * 1.1f + groundColor) * 0.5f;
    return lerp(environment, average, saturate(roughness * roughness * 0.8f));
}

// リムライト。視線と面が直角に近く、光を背にしているほど縁が明るくなります(布の毛羽立ちや産毛の光)。
float RealisticRim(float3 N, float3 V, float3 L, float power)
{
    float backLight = saturate(dot(-V, L)) * 0.6f + 0.4f;
    return pow(1.0f - saturate(dot(N, V)), power) * backLight;
}

// 頂点に接線が無いメッシュでも法線マップを使えるよう、画面上の微分から接線空間を作ります。
// T は +u、B は +v の向きになり、SP31 BumpPS の固定TBN(T=+X, B=+Z)を地面以外にも広げたものです。
float3 RealisticPerturbNormal(float3 N, float3 worldPosition, float2 uv, float3 tangentNormal)
{
    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float scale = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-20f));
    return normalize(tangentNormal.x * T * scale + tangentNormal.y * B * scale + tangentNormal.z * N);
}

// テクスチャの明るさ(輝度)を高さとみなします。
float RealisticLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

// 表示用の色は2.2乗のガンマで保存されています。光の計算は線形な値で行います。
float3 RealisticToLinear(float3 color)
{
    return pow(max(color, 0.0f), 2.2f);
}

// 明るい部分を白飛びさせずに圧縮するトーンマップ(ACES近似)の後、表示用のガンマへ戻します。
float3 RealisticToDisplay(float3 color)
{
    color = saturate((color * (2.51f * color + 0.03f)) / (color * (2.43f * color + 0.59f) + 0.14f));
    return pow(color, 1.0f / 2.2f);
}
