#pragma once

#include "renderer.h"

// Engine名前空間は、新規のEngine APIで使用する正式な名前空間です。
// 既存のグローバル型は段階移行中の互換APIとして残しています。
namespace Engine
{
	// 描画オブジェクトが共通して使うGPUリソース生成窓口。
	// 失敗時は出力先を必ず空にし、呼び出し元ごとのHRESULT処理を不要にします。
	class GraphicsResourceFactory
	{
	public:
		[[nodiscard]] static bool CreateBuffer(
			ComPtr<ID3D11Buffer>& buffer,
			const D3D11_BUFFER_DESC& description,
			const D3D11_SUBRESOURCE_DATA* initialData,
			const char* ownerName);

		// 標準のテクスチャ付き描画で使うシェーダー対をまとめて取得します。
		[[nodiscard]] static bool CreateUnlitShaderPair(
			ComPtr<ID3D11VertexShader>& vertexShader,
			ComPtr<ID3D11InputLayout>& inputLayout,
			ComPtr<ID3D11PixelShader>& pixelShader,
			const char* ownerName);

		// 描画に必要な標準リソースがすべて揃っているかを確認します。
		// GPU生成に失敗したオブジェクトは、この判定で描画だけを安全にスキップできます。
		[[nodiscard]] static bool IsDrawable(
			ID3D11Buffer* vertexBuffer,
			ID3D11InputLayout* inputLayout,
			ID3D11VertexShader* vertexShader,
			ID3D11PixelShader* pixelShader) noexcept;

		// 動的バッファのMap処理を一元化し、デバイス・バッファ・Map失敗を安全に扱います。
		[[nodiscard]] static bool MapWriteDiscard(
			ID3D11Buffer* buffer,
			D3D11_MAPPED_SUBRESOURCE& mappedResource,
			const char* ownerName);
	};
}
