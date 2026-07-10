#pragma once

#include "main.h"

#include <xaudio2.h>

#include <filesystem>
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

using TextureHandle = std::shared_ptr<const TextureResource>;
using WaveHandle = std::shared_ptr<const WaveResource>;

class ResourceManager
{
public:
	static TextureHandle LoadTexture(const std::filesystem::path& path);
	static WaveHandle LoadWave(const std::filesystem::path& path);
	static void Clear();

	static std::size_t GetTextureCount();
	static std::size_t GetWaveCount();
};
