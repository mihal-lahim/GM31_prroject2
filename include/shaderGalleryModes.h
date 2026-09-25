#pragma once

#include "surfaceMaterial.h"
#include <array>

namespace Engine
{
struct ShaderGalleryMode
{
	const wchar_t* Name;
	SurfaceShader Shader;
};

// Shared teaching catalogue. Applications choose their own controls and layout.
inline constexpr std::array<ShaderGalleryMode, 15> ShaderGalleryModes{{
	{ L"Unlit", SurfaceShader::Unlit },
	{ L"Vertex directional", SurfaceShader::VertexDirectional },
	{ L"Pixel directional", SurfaceShader::Directional },
	{ L"Blinn-Phong", SurfaceShader::BlinnPhong },
	{ L"Hemisphere", SurfaceShader::Hemisphere },
	{ L"Point", SurfaceShader::Point },
	{ L"Rim", SurfaceShader::Rim },
	{ L"Spot", SurfaceShader::Spot },
	{ L"Cook-Torrance", SurfaceShader::CookTorrance },
	{ L"Disney PBR", SurfaceShader::Disney },
	{ L"Toon steps", SurfaceShader::Toon },
	{ L"Toon ramp", SurfaceShader::ToonRamp },
	{ L"Toon outline", SurfaceShader::ToonOutline },
	{ L"Bump", SurfaceShader::Bump },
	{ L"Sample Stripes", SurfaceShader::Unlit },
}};
}
