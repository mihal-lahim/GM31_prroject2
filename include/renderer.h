#pragma once

#include "engineCommon.h"
#include <array>
#include <string>
#include <string_view>
namespace Engine
{
struct ShaderDefinition;
// Engine内で共有する標準頂点形式です。OBJ、2D、パーティクルで同じ入力レイアウトを使います。
struct VERTEX_3D
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT4 Diffuse;
	XMFLOAT2 TexCoord;
};



struct MATERIAL
{
	XMFLOAT4	Ambient;
	XMFLOAT4	Diffuse;
	XMFLOAT4	Specular;
	XMFLOAT4	Emission;
	float		Shininess;
	BOOL		TextureEnable;
	float		Dummy[2];
};



struct LIGHT
{
	BOOL		Enable;
	BOOL		Dummy[3];
	XMFLOAT4	Direction;
	XMFLOAT4	Diffuse;
	XMFLOAT4	Ambient;
	XMFLOAT4	Position;
	XMFLOAT4	PointLightparam;
	XMFLOAT4	SkyColor;
	XMFLOAT4	GroundColor;
	XMFLOAT4	GroundNormal;
	XMFLOAT4	Angle;
};

enum class RenderBlendMode
{
	Alpha,
	Additive,
	AlphaToCoverage
};

enum class PostEffectMode {
	None, Grayscale, Mosaic, MosaicAverage, CircleMosaic, Horror,
	MipPreview, Shape, RGBShift, Posterize
};

// 深度の扱いです。
// TestOnly は加算合成のエフェクト用で、壁に隠れつつ深度は汚しません。
// これが無いと、エフェクトを描くために深度を完全に切ることになり、
// 壁の手前へ描かれて壁が透けて見えます。
enum class RenderDepthMode
{
	Enabled,   // 深度テスト＋書き込み。通常の不透明物
	TestOnly,  // 深度テストのみ。半透明・加算のエフェクト
	Disabled   // 深度を使わない。画面座標のUIとデバッグ表示
};



// Direct3D 11のデバイスとフレーム開始/終了を一元管理します。
// GPUリソースの個別生成はEngine::GraphicsResourceFactoryを経由します。
class Renderer
{
private:

	static D3D_FEATURE_LEVEL       m_FeatureLevel;

	static ComPtr<ID3D11Device>           m_Device;
	static ComPtr<ID3D11DeviceContext>    m_DeviceContext;
	static ComPtr<IDXGISwapChain>         m_SwapChain;
	static ComPtr<ID3D11RenderTargetView> m_RenderTargetView;
	static ComPtr<ID3D11DepthStencilView> m_DepthStencilView;

	static ComPtr<ID3D11Buffer>			m_WorldBuffer;
	static ComPtr<ID3D11Buffer>			m_ViewBuffer;
	static ComPtr<ID3D11Buffer>			m_ProjectionBuffer;
	static ComPtr<ID3D11Buffer>			m_MaterialBuffer;
	static ComPtr<ID3D11Buffer>			m_LightBuffer;
	static ComPtr<ID3D11Buffer>			m_CameraBuffer;
	static ComPtr<ID3D11Buffer>			m_ParameterBuffer;
	static ComPtr<ID3D11Buffer>			m_PostOptionsBuffer;
	static ComPtr<ID3D11Texture2D>		m_SceneTexture;
	static ComPtr<ID3D11RenderTargetView> m_SceneRenderTarget;
	static ComPtr<ID3D11ShaderResourceView> m_SceneResource;
	static ComPtr<ID3D11Buffer>			m_PostVertexBuffer;
public:
	// 複数パスのポスト効果用の作業バッファの枠数。枠 0,1 は 1/2、2,3 は 1/4、4,5 は 1/8 解像度。
	static constexpr std::size_t PostWorkSlots = 6;
	static constexpr int PostWorkSlotShift(std::size_t slot) { return static_cast<int>(slot / 2) + 1; }
private:
	static std::array<ComPtr<ID3D11Texture2D>, PostWorkSlots> m_PostWorkTextures;
	static std::array<ComPtr<ID3D11RenderTargetView>, PostWorkSlots> m_PostWorkTargets;
	static std::array<ComPtr<ID3D11ShaderResourceView>, PostWorkSlots> m_PostWorkResources;
	static PostEffectMode m_PostEffectMode;
	static std::string m_PostDefinitionId;
	static void (*m_OverlayCallback)();
	static XMFLOAT4 m_PostOptions;
	static XMFLOAT4 m_PostExtraOptions;
	static bool m_CustomPostOptions;
	static XMFLOAT4 m_PostDefinitionParameters;
	static XMFLOAT4 m_ClearColor;


	static ComPtr<ID3D11DepthStencilState> m_DepthStateEnable;
	static ComPtr<ID3D11DepthStencilState> m_DepthStateTestOnly;
	static ComPtr<ID3D11DepthStencilState> m_DepthStateDisable;

	static ComPtr<ID3D11BlendState>		m_BlendState;
	static ComPtr<ID3D11BlendState>		m_BlendStateAdd;
	static ComPtr<ID3D11BlendState>		m_BlendStateATC;
	static ComPtr<ID3D11RasterizerState> m_RasterStateBack;
	static ComPtr<ID3D11RasterizerState> m_RasterStateFront;
	static RenderDepthMode m_DepthMode;
	static RenderBlendMode m_BlendMode;
	// 影パスが太陽の向きを知るため、最後に設定したライトを覚えておきます。
	static LIGHT m_Light;



	static void DrawPostDefinition(const ShaderDefinition& definition);

public:
	static constexpr std::size_t MaxPostPasses = 16;

	static bool Init();
	static void Uninit();
	// Beginでフレーム共通状態を初期化し、EndでPresentします。
	static void Begin();
	static void End();
	static void SetOverlayCallback(void (*callback)()) { m_OverlayCallback = callback; }
	static constexpr XMFLOAT4 DefaultClearColor{ 0.42f, 0.62f, 0.82f, 1.0f };
	// Beginで画面を塗りつぶす色です。シーンを抜けるときはDefaultClearColorへ戻してください。
	static void SetClearColor(XMFLOAT4 color) { m_ClearColor = color; }

	static void SetDepthMode(RenderDepthMode Mode);
	// 描画中の深度だけを消します。一人称の手持ち武器のように、壁へめり込ませず
	// 常に手前へ描きたいものを、ワールドを描き終えた後で描く直前に呼びます。
	static void ClearDepth();
	static RenderDepthMode GetDepthMode() { return m_DepthMode; }
	// 既存コード向けの互換APIです。新規コードはSetDepthModeを使用します。
	static void SetDepthEnable(bool Enable)
	{
		SetDepthMode(Enable ? RenderDepthMode::Enabled : RenderDepthMode::Disabled);
	}
	static bool GetDepthEnabled() { return m_DepthMode != RenderDepthMode::Disabled; }
	static void SetATCEnable(bool Enable);
	static void SetWorldViewProjection2D();
	static void SetWorldMatrix(XMMATRIX WorldMatrix);
	static void SetViewMatrix(XMMATRIX ViewMatrix);
	static void SetProjectionMatrix(XMMATRIX ProjectionMatrix);
	static void SetMaterial(MATERIAL Material);
	static void SetLight(LIGHT Light);
	static const LIGHT& GetLight() { return m_Light; }
	static void SetCameraPosition(XMFLOAT3 CameraPosition);
	static void SetParameter(XMFLOAT4 Parameter);
	static void SetPostEffect(PostEffectMode mode) { m_PostEffectMode = mode; m_PostDefinitionId.clear(); m_CustomPostOptions = false; }
	static void SetPostEffectParameters(XMFLOAT4 options, XMFLOAT4 extra = {})
	{
		m_PostOptions = options; m_PostExtraOptions = extra; m_CustomPostOptions = true;
	}
	static PostEffectMode GetPostEffect() { return m_PostEffectMode; }
	static bool SetPostEffectDefinition(std::string_view id);
	static bool SetPostEffectFloat(std::string_view name, float value);
	static std::string_view GetPostEffectDefinitionId() { return m_PostDefinitionId; }
	static void SetAddBlendEnable(bool Enable);
	static void SetBlendMode(RenderBlendMode mode);
	static void SetCullFront(bool enabled);
	static RenderBlendMode GetBlendMode() { return m_BlendMode; }

	static ID3D11Device* GetDevice( void ){ return m_Device.Get(); }
	static ID3D11DeviceContext* GetDeviceContext( void ){ return m_DeviceContext.Get(); }
	static ID3D11ShaderResourceView* GetPostSource() { return m_SceneResource.Get(); }



	// ResourceManager経由でシェーダーを取得します。失敗時は出力を空にしてfalseを返します。
	static bool CreateVertexShader(ComPtr<ID3D11VertexShader>& VertexShader, ComPtr<ID3D11InputLayout>& VertexLayout, const char* FileName);
	static bool CreatePixelShader(ComPtr<ID3D11PixelShader>& PixelShader, const char* FileName);


};

// GameObjectごとのDrawがパイプライン状態を変更しても、次のDrawへ漏らさないためのRAIIです。
class RendererStateScope
{
private:
	struct Snapshot;
	std::unique_ptr<Snapshot> m_Snapshot;
	RenderDepthMode m_DepthMode;
	RenderBlendMode m_BlendMode;

public:
	RendererStateScope();
	~RendererStateScope();
	RendererStateScope(const RendererStateScope&) = delete;
	RendererStateScope& operator=(const RendererStateScope&) = delete;

	// 取得済みスナップショットへ何度でも復元できます。
	// RenderQueueが1回のスナップショットを各Draw後に使い回すために公開しています。
	void Restore() const;
};
}

// 既存ゲームコード向けの互換名です。新規コードはEngine::を使用します。
using VERTEX_3D = Engine::VERTEX_3D;
using MATERIAL = Engine::MATERIAL;
using LIGHT = Engine::LIGHT;
using RenderBlendMode = Engine::RenderBlendMode;
using PostEffectMode = Engine::PostEffectMode;
using RenderDepthMode = Engine::RenderDepthMode;
using Renderer = Engine::Renderer;
using RendererStateScope = Engine::RendererStateScope;
