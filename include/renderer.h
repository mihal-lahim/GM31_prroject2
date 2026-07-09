#pragma once





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



public:
	static void Init();
	static void Uninit();
	static void Begin();
	static void End();

	static void SetDepthEnable(bool Enable);
	static void SetATCEnable(bool Enable);
	static void SetWorldViewProjection2D();
	static void SetWorldMatrix(XMMATRIX WorldMatrix);
	static void SetViewMatrix(XMMATRIX ViewMatrix);
	static void SetProjectionMatrix(XMMATRIX ProjectionMatrix);
	static void SetMaterial(MATERIAL Material);
	static void SetLight(LIGHT Light);
	static void SetAddBlendEnable(bool Enable);

	static ID3D11Device* GetDevice( void ){ return m_Device.Get(); }
	static ID3D11DeviceContext* GetDeviceContext( void ){ return m_DeviceContext.Get(); }



	static void CreateVertexShader(ComPtr<ID3D11VertexShader>& VertexShader, ComPtr<ID3D11InputLayout>& VertexLayout, const char* FileName);
	static void CreatePixelShader(ComPtr<ID3D11PixelShader>& PixelShader, const char* FileName);


};
