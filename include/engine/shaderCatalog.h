#pragma once

#include <array>
#include <span>
#include <string_view>

#include "engineCommon.h"

namespace Engine
{
// SP31 と GM31 の移植用規約は shaders/surfaces/PortableCommon.hlsli を参照。
// 新しい効果は shaders/surfaces/ または shaders/post/ に置き、この表に登録する。
enum class ShaderUsage { Surface, Post };
enum class ShaderCull { Back, Front };

struct ShaderPass
{
	std::string_view VertexFile;
	std::string_view PixelFile;
	ShaderCull Cull = ShaderCull::Back;
	bool OverrideParameters = false;
	XMFLOAT4 Parameters{};
	// 複数パスのポスト効果で、最後以外のパスが書く作業バッファの枠(Renderer::PostWorkSlots)。
	// 負なら 1/4 解像度の2枠(2, 3)を交互に使う。
	int PostOutput = -1;
};

struct ShaderDefinition
{
	std::string_view Id;
	const wchar_t* DisplayName;
	ShaderUsage Usage;
	std::span<const ShaderPass> Passes;
	std::array<std::string_view, 4> ParameterNames;
	XMFLOAT4 Defaults;
	int LegacyMode = -1;
	bool GenerateMips = false;
	std::array<std::string_view, 2> TextureNames{ "albedo", "detail" };
};

inline constexpr std::array<ShaderPass, 1> SampleStripesPasses{{
	{ "SampleStripesVS.cso", "sampleStripesPS.cso" }
}};
inline constexpr std::array<ShaderPass, 2> ToonOutlinePasses{{
	{ "spToonOutlineVS.cso", "spToonOutlinePS.cso", ShaderCull::Front, true,
		{ 0.075f, 0.0f, 0.0f, 0.0f } },
	{ "classroomLitVS.cso", "spSurfacePS.cso" }
}};
inline constexpr std::array<ShaderPass, 1> WaterPasses{{
	{ "classroomLitVS.cso", "classroomWaterPS.cso" }
}};
inline constexpr std::array<ShaderPass, 1> RealisticPasses{{
	{ "classroomLitVS.cso", "realisticPS.cso" }
}};
inline constexpr std::array<ShaderPass, 1> RealisticWaterPasses{{
	{ "classroomLitVS.cso", "realisticWaterPS.cso" }
}};
inline constexpr std::array<ShaderPass, 1> RealisticFoliagePasses{{
	{ "classroomLitVS.cso", "realisticFoliagePS.cso" }
}};
inline constexpr std::array<ShaderPass, 1> RealisticArenaPasses{{
	{ "classroomLitVS.cso", "realisticArenaPS.cso" }
}};
inline constexpr std::array<ShaderPass, 1> InvertPostPasses{{
	{ "spPostVS.cso", "invertPostPS.cso" }
}};
inline constexpr std::array<ShaderPass, 1> TimeSlowPostPasses{{
	{ "spPostVS.cso", "timeSlowPS.cso" }
}};

// 複数パスのポスト効果は最後のパスだけ画面へ描き、それ以前は作業バッファの枠
// (PostOutput)へ描く。各パスは t0=元画面、t1=直前の出力、t2〜t7=作業枠0〜5、
// b7=そのパスの Parameters を受け取る。
// ブルームは SP31 と同じ3段階方式。段階ごとに、その解像度のミップから明部を抽出し、
// 横・縦のガウスぼかし(1.5テクセル間隔)を2回ずつかけて枠 0(1/2)・2(1/4)・4(1/8)に残す。
#define GM31_BLOOM_LEVEL(mip, slot) \
	{ "spPostVS.cso", "bloomExtractPS.cso", ShaderCull::Back, false, { mip, 0.0f, 0.0f, 0.0f }, slot }, \
	{ "spPostVS.cso", "bloomBlurPS.cso", ShaderCull::Back, false, { 1.5f, 0.0f, 0.0f, 0.0f }, slot + 1 }, \
	{ "spPostVS.cso", "bloomBlurPS.cso", ShaderCull::Back, false, { 0.0f, 1.5f, 0.0f, 0.0f }, slot }, \
	{ "spPostVS.cso", "bloomBlurPS.cso", ShaderCull::Back, false, { 1.5f, 0.0f, 0.0f, 0.0f }, slot + 1 }, \
	{ "spPostVS.cso", "bloomBlurPS.cso", ShaderCull::Back, false, { 0.0f, 1.5f, 0.0f, 0.0f }, slot }
inline constexpr std::array<ShaderPass, 16> BloomPostPasses{{
	GM31_BLOOM_LEVEL(1.0f, 0),
	GM31_BLOOM_LEVEL(2.0f, 2),
	GM31_BLOOM_LEVEL(3.0f, 4),
	{ "spPostVS.cso", "bloomCompositePS.cso" }
}};
#undef GM31_BLOOM_LEVEL

// SP31 のギャラリー用シェーダーをそのまま移植したもの(shaders/surfaces/Sp31*.hlsl)。
// spSurfacePS の近似版と違い、SP31 と同じ式で描きます。
inline constexpr std::array<ShaderPass, 1> Sp31UnlitPasses{{ { "Sp31PixelLitVS.cso", "Sp31UnlitTexturePS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31VertexDirectionalPasses{{
	{ "Sp31VertexDirectionalVS.cso", "Sp31VertexDirectionalPS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31DirectionalPasses{{ { "Sp31PixelLitVS.cso", "Sp31DirectionalPS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31BlinnPhongPasses{{ { "Sp31PixelLitVS.cso", "Sp31BlinnPhongPS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31HemispherePasses{{ { "Sp31PixelLitVS.cso", "Sp31HemispherePS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31PointPasses{{ { "Sp31PixelLitVS.cso", "Sp31PointPS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31RimPasses{{ { "Sp31PixelLitVS.cso", "Sp31RimPS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31SpotPasses{{ { "Sp31PixelLitVS.cso", "Sp31SpotPS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31CookTorrancePasses{{ { "Sp31PixelLitVS.cso", "Sp31CookPS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31DisneyPasses{{ { "Sp31PixelLitVS.cso", "Sp31DisneyPS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31ToonStepsPasses{{ { "Sp31PixelLitVS.cso", "Sp31Toon1PS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31ToonRampPasses{{ { "Sp31PixelLitVS.cso", "Sp31Toon2PS.cso" } }};
// 背面法: 法線方向へ Parameter.w (モデル空間の長さ)だけ膨らませた裏面を描いてから本体を描く。
inline constexpr std::array<ShaderPass, 2> Sp31ToonBackfacePasses{{
	{ "Sp31ToonBackfaceVS.cso", "Sp31ToonBackfacePS.cso", ShaderCull::Front },
	{ "Sp31PixelLitVS.cso", "Sp31Toon2PS.cso" }
}};
inline constexpr std::array<ShaderPass, 1> Sp31SpotFieldPasses{{ { "Sp31PixelLitVS.cso", "Sp31SpotLightingPS.cso" } }};
inline constexpr std::array<ShaderPass, 1> Sp31BumpFieldPasses{{ { "Sp31PixelLitVS.cso", "Sp31BumpPS.cso" } }};

inline constexpr std::array<ShaderDefinition, 25> ShaderCatalog{{
	{ "sample-stripes", L"Sample Stripes", ShaderUsage::Surface,
		SampleStripesPasses,
		{ "stripeCount", "contrast", "lightMix", "phase" }, { 12.0f, 0.75f, 0.7f, 0.0f } },
	{ "toon-outline", L"Toon Outline", ShaderUsage::Surface,
		ToonOutlinePasses,
		{ "bands", "rimThreshold", "rimDarkness", {} }, { 4.0f, 0.02f, 0.65f, 0.0f }, 15 },
	{ "water", L"Water", ShaderUsage::Surface,
		WaterPasses,
		{ "waveScale", "fresnel", {}, {} }, { 0.45f, 0.0f, 1.0f, 0.0f } },
	// SP31 の照明教材(Disney拡散・Cook-Torrance・半球ライト・リム・バンプ)を組み合わせた写実表現。
	{ "realistic", L"Realistic", ShaderUsage::Surface,
		RealisticPasses,
		{ "roughness", "metallic", "bump", "rim" }, { 0.6f, 0.0f, 0.0f, 0.0f }, -1, false,
		{ "albedo", "normal" } },
	// 同じ部品で作った水面(映り込み・きらめき・透け)と、植物のビルボード(透過光・陰り)。
	{ "realistic-water", L"Realistic Water", ShaderUsage::Surface,
		RealisticWaterPasses,
		{ "time", "ripple", "roughness", {} }, { 0.0f, 1.0f, 0.08f, 0.0f } },
	{ "realistic-foliage", L"Realistic Foliage", ShaderUsage::Surface,
		RealisticFoliagePasses,
		{}, { 0.0f, 0.0f, 0.0f, 0.0f } },
	// 同じ照明で、テクスチャを使わずワールド座標から石材・コンクリートのブロック(目地・ムラ・接地の陰)を作る面。
	// 素地の色はモデルの材質色。SlowAction の闘技場(床・壁・障害物)が使う。
	{ "realistic-arena", L"Realistic Arena", ShaderUsage::Surface,
		RealisticArenaPasses,
		{ "roughness", "metallic", "tileScale", "weathering" }, { 0.85f, 0.0f, 1.0f, 0.6f } },
	{ "post-invert", L"Invert", ShaderUsage::Post,
		InvertPostPasses,
		{ "strength", {}, {}, {} }, { 1.0f, 0.0f, 0.0f, 0.0f }, -1, false,
		{ "scene", {} } },
	// SlowAction の時間減速(彩度低下・寒色・ビネット・外周の色ずれ・衝撃波・ジャスト減速のフラッシュ)。
	// 見た目の強さ(彩度・寒色・ビネット)は SetPostEffectParameters の extra(x, y, w)で渡す
	// (渡さなければシェーダーの標準値)。既定値は減速しきった状態の見た目。
	{ "post-time-slow", L"Time Slow", ShaderUsage::Post,
		TimeSlowPostPasses,
		{ "intensity", "waveProgress", "flash", "waveSign" }, { 1.0f, 0.0f, 0.0f, 1.0f }, -1, false,
		{ "scene", {} } },
	// SP31 と同じ3段階ブルーム。既定値は SP31 課題06の参考画像の設定(Luminance / Blend 1〜3)。
	{ "post-bloom", L"Bloom", ShaderUsage::Post,
		BloomPostPasses,
		{ "luminance", "blendFine", "blendMiddle", "blendWide" }, { 0.55f, 1.22f, 1.21f, 2.0f }, -1, true,
		{ "scene", {} } },
	// 以下は SP31 のギャラリー既定値。ライトは Renderer::SetLight の値をそのまま使う。
	{ "sp31-unlit", L"SP31 Unlit", ShaderUsage::Surface, Sp31UnlitPasses, {}, {} },
	{ "sp31-vertex-directional", L"SP31 Vertex Directional", ShaderUsage::Surface,
		Sp31VertexDirectionalPasses, {}, {} },
	{ "sp31-directional", L"SP31 Pixel Directional", ShaderUsage::Surface, Sp31DirectionalPasses, {}, {} },
	{ "sp31-blinn-phong", L"SP31 Blinn-Phong", ShaderUsage::Surface, Sp31BlinnPhongPasses, {}, {} },
	{ "sp31-hemisphere", L"SP31 Hemisphere", ShaderUsage::Surface, Sp31HemispherePasses, {}, {} },
	{ "sp31-point", L"SP31 Point", ShaderUsage::Surface, Sp31PointPasses, {}, {} },
	{ "sp31-rim", L"SP31 Rim", ShaderUsage::Surface, Sp31RimPasses, {}, {} },
	{ "sp31-spot", L"SP31 Spot", ShaderUsage::Surface, Sp31SpotPasses, {}, {} },
	{ "sp31-cook-torrance", L"SP31 Cook-Torrance", ShaderUsage::Surface, Sp31CookTorrancePasses,
		{ "roughness", "metallic", {}, {} }, { 0.35f, 0.5f, 0.0f, 0.0f } },
	{ "sp31-disney", L"SP31 Disney PBR", ShaderUsage::Surface, Sp31DisneyPasses,
		{ "roughness", "metallic", "lightCount", {} }, { 0.5f, 0.2f, 3.0f, 0.0f } },
	{ "sp31-toon-steps", L"SP31 Toon Steps", ShaderUsage::Surface, Sp31ToonStepsPasses,
		{ "darkEdge", "brightEdge", "outlineWidth", "outlineDarkness" }, { 0.4f, 0.7f, 0.25f, 0.3f } },
	{ "sp31-toon-ramp", L"SP31 Toon Ramp", ShaderUsage::Surface, Sp31ToonRampPasses,
		{ "rampRow", "outlineWidth", "outlineDarkness", {} }, { 8.0f / 256.0f, 0.25f, 0.3f, 0.0f },
		-1, false, { "albedo", "ramp" } },
	{ "sp31-toon-backface", L"SP31 Toon Backface Outline", ShaderUsage::Surface, Sp31ToonBackfacePasses,
		{ "rampRow", "outlineWidth", "outlineDarkness", "backfaceWidth" }, { 8.0f / 256.0f, 0.25f, 0.3f, 0.08f },
		-1, false, { "albedo", "ramp" } },
	// SP31 の Field3D(スポットライトの床)と BumpField3D(法線マップの床)。
	{ "sp31-spot-field", L"SP31 Spot Field", ShaderUsage::Surface, Sp31SpotFieldPasses, {}, {} },
	{ "sp31-bump-field", L"SP31 Bump Field", ShaderUsage::Surface, Sp31BumpFieldPasses, {}, {},
		-1, false, { "albedo", "normal" } },
}};

inline const ShaderDefinition* FindShaderDefinition(std::string_view id)
{
	for (const ShaderDefinition& definition : ShaderCatalog)
		if (definition.Id == id) return &definition;
	return nullptr;
}
}
