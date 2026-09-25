#pragma once

#include "engineCommon.h"
#include <string>
#include <string_view>
namespace Engine
{
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
	static ComPtr<ID3D11Texture2D>		m_SceneTexture;
	static ComPtr<ID3D11RenderTargetView> m_SceneRenderTarget;
	static ComPtr<ID3D11ShaderResourceView> m_SceneResource;
	static ComPtr<ID3D11Buffer>			m_PostVertexBuffer;
	static PostEffectMode m_PostEffectMode;
	static std::string m_PostDefinitionId;
	static XMFLOAT4 m_PostDefinitionParameters;


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



public:
	static bool Init();
	static void Uninit();
	// Beginでフレーム共通状態を初期化し、EndでPresentします。
	static void Begin();
	static void End();

	static void SetDepthMode(RenderDepthMode Mode);
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
	static void SetCameraPosition(XMFLOAT3 CameraPosition);
	static void SetParameter(XMFLOAT4 Parameter);
	static void SetPostEffect(PostEffectMode mode) { m_PostEffectMode = mode; m_PostDefinitionId.clear(); }
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
