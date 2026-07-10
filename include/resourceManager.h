#pragma once

#include "main.h"

#include <xaudio2.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct TextureResource
{
	ComPtr<ID3D11ShaderResourceView> View;

	ID3D11ShaderResourceView* Get() const { return View.Get(); }
};

struct WaveResource
{
	std::vector<BYTE> FormatData;
	std::vector<BYTE> AudioData;
	UINT32 PlayLength = 0;

	const WAVEFORMATEX* GetFormat() const
	{
		return FormatData.size() >= sizeof(WAVEFORMATEX)
			? reinterpret_cast<const WAVEFORMATEX*>(FormatData.data())
			: nullptr;
	}
};

struct VertexShaderResource
{
	ComPtr<ID3D11InputLayout> InputLayout;
	ComPtr<ID3D11VertexShader> Shader;
};

struct PixelShaderResource
{
	ComPtr<ID3D11PixelShader> Shader;
};

struct MODEL;

using TextureHandle = std::shared_ptr<const TextureResource>;
using WaveHandle = std::shared_ptr<const WaveResource>;
using VertexShaderHandle = std::shared_ptr<const VertexShaderResource>;
using PixelShaderHandle = std::shared_ptr<const PixelShaderResource>;
using ModelHandle = std::shared_ptr<const MODEL>;
using ModelFactory = std::function<std::shared_ptr<MODEL>()>;

class ResourceManager
{
public:
	static TextureHandle LoadTexture(const std::filesystem::path& path);
	static WaveHandle LoadWave(const std::filesystem::path& path);
	static VertexShaderHandle LoadVertexShader(const std::filesystem::path& path);
	static PixelShaderHandle LoadPixelShader(const std::filesystem::path& path);
	static ModelHandle LoadModel(const std::filesystem::path& path, const ModelFactory& factory);
	static void Clear();

	static std::size_t GetTextureCount();
	static std::size_t GetWaveCount();
	static std::size_t GetVertexShaderCount();
	static std::size_t GetPixelShaderCount();
	static std::size_t GetModelCount();
};
