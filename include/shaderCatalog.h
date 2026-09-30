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
inline constexpr std::array<ShaderPass, 1> InvertPostPasses{{
	{ "spPostVS.cso", "invertPostPS.cso" }
}};

inline constexpr std::array<ShaderDefinition, 7> ShaderCatalog{{
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
	{ "post-invert", L"Invert", ShaderUsage::Post,
		InvertPostPasses,
		{ "strength", {}, {}, {} }, { 1.0f, 0.0f, 0.0f, 0.0f }, -1, false,
		{ "scene", {} } },
}};

inline const ShaderDefinition* FindShaderDefinition(std::string_view id)
{
	for (const ShaderDefinition& definition : ShaderCatalog)
		if (definition.Id == id) return &definition;
	return nullptr;
}
}
