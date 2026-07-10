#include "resourceManager.h"

#include "renderer.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <unordered_map>

namespace
{
	std::unordered_map<std::string, std::shared_ptr<TextureResource>> g_TextureCache;
	std::unordered_map<std::string, std::shared_ptr<WaveResource>> g_WaveCache;

	std::string MakeCacheKey(const std::filesystem::path& path)
	{
		std::error_code error;
		std::filesystem::path normalizedPath = std::filesystem::absolute(path, error);
		if (error)
		{
			normalizedPath = path;
		}

		std::string key = normalizedPath.lexically_normal().generic_string();
		std::transform(key.begin(), key.end(), key.begin(), [](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});
		return key;
	}

	void DebugResourceError(const char* resourceType, const std::filesystem::path& path, const char* reason)
	{
		const std::string message = std::string("ResourceManager: failed to load ")
			+ resourceType + " '" + path.string() + "': " + reason + "\n";
		OutputDebugStringA(message.c_str());
	}

	class MmioFile
	{
	private:
		HMMIO m_Handle = nullptr;

	public:
		explicit MmioFile(const std::filesystem::path& path)
		{
			m_Handle = mmioOpenW(const_cast<LPWSTR>(path.c_str()), nullptr, MMIO_READ);
		}

		~MmioFile()
		{
			if (m_Handle != nullptr)
			{
				mmioClose(m_Handle, 0);
			}
		}

		HMMIO Get() const { return m_Handle; }
		explicit operator bool() const { return m_Handle != nullptr; }
	};
}

TextureHandle ResourceManager::LoadTexture(const std::filesystem::path& path)
{
	if (path.empty())
	{
		return {};
	}

	const std::string key = MakeCacheKey(path);
	if (const auto found = g_TextureCache.find(key); found != g_TextureCache.end())
	{
		return found->second;
	}

	if (Renderer::GetDevice() == nullptr)
	{
		DebugResourceError("texture", path, "renderer is not initialized");
		return {};
	}

	TexMetadata metadata{};
	ScratchImage image{};
	HRESULT result = LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, &metadata, image);
	if (FAILED(result))
	{
		DebugResourceError("texture", path, "image decode failed");
		return {};
	}

	auto resource = std::make_shared<TextureResource>();
	result = CreateShaderResourceView(
		Renderer::GetDevice(),
		image.GetImages(),
		image.GetImageCount(),
		metadata,
		resource->View.GetAddressOf());
	if (FAILED(result) || resource->View == nullptr)
	{
		DebugResourceError("texture", path, "GPU resource creation failed");
		return {};
	}

	g_TextureCache.emplace(key, resource);
	return resource;
}

WaveHandle ResourceManager::LoadWave(const std::filesystem::path& path)
{
	if (path.empty())
	{
		return {};
	}

	const std::string key = MakeCacheKey(path);
	if (const auto found = g_WaveCache.find(key); found != g_WaveCache.end())
	{
		return found->second;
	}

	MmioFile file(path);
	if (!file)
	{
		DebugResourceError("wave", path, "file open failed");
		return {};
	}

	MMCKINFO riffChunk{};
	riffChunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	if (mmioDescend(file.Get(), &riffChunk, nullptr, MMIO_FINDRIFF) != MMSYSERR_NOERROR)
	{
		DebugResourceError("wave", path, "RIFF WAVE chunk was not found");
		return {};
	}

	MMCKINFO formatChunk{};
	formatChunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (mmioDescend(file.Get(), &formatChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR
		|| formatChunk.cksize < sizeof(PCMWAVEFORMAT)
		|| formatChunk.cksize > static_cast<DWORD>((std::numeric_limits<LONG>::max)()))
	{
		DebugResourceError("wave", path, "invalid format chunk");
		return {};
	}

	auto resource = std::make_shared<WaveResource>();
	const std::size_t formatSize = (std::max)(static_cast<std::size_t>(formatChunk.cksize), sizeof(WAVEFORMATEX));
	resource->FormatData.resize(formatSize);
	const LONG formatRead = mmioRead(
		file.Get(),
		reinterpret_cast<HPSTR>(resource->FormatData.data()),
		static_cast<LONG>(formatChunk.cksize));
	if (formatRead != static_cast<LONG>(formatChunk.cksize))
	{
		DebugResourceError("wave", path, "format chunk read failed");
		return {};
	}
	if (formatChunk.cksize < sizeof(WAVEFORMATEX))
	{
		reinterpret_cast<WAVEFORMATEX*>(resource->FormatData.data())->cbSize = 0;
	}

	mmioAscend(file.Get(), &formatChunk, 0);
	const WAVEFORMATEX* format = resource->GetFormat();
	if (format == nullptr || format->nBlockAlign == 0)
	{
		DebugResourceError("wave", path, "invalid block alignment");
		return {};
	}

	MMCKINFO dataChunk{};
	dataChunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if (mmioDescend(file.Get(), &dataChunk, &riffChunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR
		|| dataChunk.cksize == 0
		|| dataChunk.cksize > static_cast<DWORD>((std::numeric_limits<LONG>::max)()))
	{
		DebugResourceError("wave", path, "invalid data chunk");
		return {};
	}

	resource->AudioData.resize(dataChunk.cksize);
	const LONG dataRead = mmioRead(
		file.Get(),
		reinterpret_cast<HPSTR>(resource->AudioData.data()),
		static_cast<LONG>(dataChunk.cksize));
	if (dataRead <= 0)
	{
		DebugResourceError("wave", path, "audio data read failed");
		return {};
	}
	resource->AudioData.resize(static_cast<std::size_t>(dataRead));
	resource->PlayLength = static_cast<UINT32>(resource->AudioData.size() / format->nBlockAlign);
	if (resource->PlayLength == 0)
	{
		DebugResourceError("wave", path, "audio data is too short");
		return {};
	}

	g_WaveCache.emplace(key, resource);
	return resource;
}

void ResourceManager::Clear()
{
	g_TextureCache.clear();
	g_WaveCache.clear();
}

std::size_t ResourceManager::GetTextureCount()
{
	return g_TextureCache.size();
}

std::size_t ResourceManager::GetWaveCount()
{
	return g_WaveCache.size();
}
