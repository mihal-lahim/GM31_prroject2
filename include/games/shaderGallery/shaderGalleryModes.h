#pragma once

#include "surfaceMaterial.h"
#include <array>
#include <string_view>

namespace Engine
{
struct ShaderGalleryMode
{
	const wchar_t* Name;
	SurfaceShader Shader;
	// 登録済みのシェーダー定義。空なら Shader の旧経路(spSurfacePS)で描きます。
	std::string_view DefinitionId;
};

// Shared teaching catalogue. Applications choose their own controls and layout.
// SP31 にある方式は SP31 のシェーダーを移植した sp31-* 定義で描きます。
inline constexpr std::array<ShaderGalleryMode, 15> ShaderGalleryModes{{
	{ L"Unlit", SurfaceShader::Unlit, "sp31-unlit" },
	{ L"Vertex directional", SurfaceShader::VertexDirectional, "sp31-vertex-directional" },
	{ L"Pixel directional", SurfaceShader::Directional, "sp31-directional" },
	{ L"Blinn-Phong", SurfaceShader::BlinnPhong, "sp31-blinn-phong" },
	{ L"Hemisphere", SurfaceShader::Hemisphere, "sp31-hemisphere" },
	{ L"Point", SurfaceShader::Point, "sp31-point" },
	{ L"Rim", SurfaceShader::Rim, "sp31-rim" },
	{ L"Spot", SurfaceShader::Spot, "sp31-spot" },
	{ L"Cook-Torrance", SurfaceShader::CookTorrance, "sp31-cook-torrance" },
	{ L"Disney PBR", SurfaceShader::Disney, "sp31-disney" },
	{ L"Toon steps", SurfaceShader::Toon, "sp31-toon-steps" },
	{ L"Toon ramp", SurfaceShader::ToonRamp, "sp31-toon-ramp" },
	{ L"Toon outline", SurfaceShader::ToonOutline, "sp31-toon-backface" },
	// 法線マップは GM31 で追加した方式(SP31 のギャラリーには無い)。
	{ L"Bump", SurfaceShader::Bump, {} },
	{ L"Sample Stripes", SurfaceShader::Unlit, "sample-stripes" },
}};
}
