#pragma once

#include "engineCommon.h"

#include <xaudio2.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct MODEL;

namespace Engine
{
// 不変リソースはshared_ptrハンドルで共有し、呼び出し側にGPU所有権を渡しません。
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

using TextureHandle = std::shared_ptr<const TextureResource>;
using WaveHandle = std::shared_ptr<const WaveResource>;
using VertexShaderHandle = std::shared_ptr<const VertexShaderResource>;
using PixelShaderHandle = std::shared_ptr<const PixelShaderResource>;
using ModelHandle = std::shared_ptr<const ::MODEL>;
using ModelFactory = std::function<std::shared_ptr<::MODEL>()>;

// パスを正規化して各種リソースを一度だけ読み込むキャッシュです。
// Clear後も既存ハンドルは有効なため、Scene破棄とGPU終了の順序を保てます。
class ResourceManager
{
public:
	static TextureHandle LoadTexture(const std::filesystem::path& path);
	static WaveHandle LoadWave(const std::filesystem::path& path);
	static VertexShaderHandle LoadVertexShader(const std::filesystem::path& path);
	static PixelShaderHandle LoadPixelShader(const std::filesystem::path& path);
	static ModelHandle LoadModel(const std::filesystem::path& path, const ModelFactory& factory);
	static void Clear();
	// どのハンドルからも参照されなくなったモデル・テクスチャ・WAVを解放し、解放件数を返します。
	// Scene切替直後に呼ぶ想定です(新Sceneが掴んだものは参照が残るので落ちません)。
	static std::size_t CollectUnused();

	static std::size_t GetTextureCount();
	static std::size_t GetWaveCount();
	static std::size_t GetVertexShaderCount();
	static std::size_t GetPixelShaderCount();
	static std::size_t GetModelCount();
};
}

// 既存ゲームコード向けの互換名です。新規コードはEngine::を使用します。
using TextureResource = Engine::TextureResource;
using WaveResource = Engine::WaveResource;
using VertexShaderResource = Engine::VertexShaderResource;
using PixelShaderResource = Engine::PixelShaderResource;
using TextureHandle = Engine::TextureHandle;
using WaveHandle = Engine::WaveHandle;
using VertexShaderHandle = Engine::VertexShaderHandle;
using PixelShaderHandle = Engine::PixelShaderHandle;
using ModelHandle = Engine::ModelHandle;
using ModelFactory = Engine::ModelFactory;
using ResourceManager = Engine::ResourceManager;
