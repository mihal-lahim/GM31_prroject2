#pragma once

#include "engineCommon.h"
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
};

enum class RenderBlendMode
{
	Alpha,
	Additive,
	AlphaToCoverage
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


	static ComPtr<ID3D11DepthStencilState> m_DepthStateEnable;
	static ComPtr<ID3D11DepthStencilState> m_DepthStateDisable;

	static ComPtr<ID3D11BlendState>		m_BlendState;
	static ComPtr<ID3D11BlendState>		m_BlendStateAdd;
	static ComPtr<ID3D11BlendState>		m_BlendStateATC;
	static bool m_DepthEnabled;
	static RenderBlendMode m_BlendMode;



public:
	static bool Init();
	static void Uninit();
	// Beginでフレーム共通状態を初期化し、EndでPresentします。
	static void Begin();
	static void End();

	static void SetDepthEnable(bool Enable);
	static bool GetDepthEnabled() { return m_DepthEnabled; }
	static void SetATCEnable(bool Enable);
	static void SetWorldViewProjection2D();
	static void SetWorldMatrix(XMMATRIX WorldMatrix);
	static void SetViewMatrix(XMMATRIX ViewMatrix);
	static void SetProjectionMatrix(XMMATRIX ProjectionMatrix);
	static void SetMaterial(MATERIAL Material);
	static void SetLight(LIGHT Light);
	static void SetAddBlendEnable(bool Enable);
	static void SetBlendMode(RenderBlendMode mode);
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
	bool m_DepthEnabled;
	RenderBlendMode m_BlendMode;

public:
	RendererStateScope();
	~RendererStateScope();
	RendererStateScope(const RendererStateScope&) = delete;
	RendererStateScope& operator=(const RendererStateScope&) = delete;
};
