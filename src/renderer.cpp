
#include "main.h"
#include "renderer.h"
#include "resourceManager.h"
#include <array>

namespace Engine
{

D3D_FEATURE_LEVEL       Renderer::m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;

ComPtr<ID3D11Device>           Renderer::m_Device{};
ComPtr<ID3D11DeviceContext>    Renderer::m_DeviceContext{};
ComPtr<IDXGISwapChain>         Renderer::m_SwapChain{};
ComPtr<ID3D11RenderTargetView> Renderer::m_RenderTargetView{};
ComPtr<ID3D11DepthStencilView> Renderer::m_DepthStencilView{};

ComPtr<ID3D11Buffer>			Renderer::m_WorldBuffer{};
ComPtr<ID3D11Buffer>			Renderer::m_ViewBuffer{};
ComPtr<ID3D11Buffer>			Renderer::m_ProjectionBuffer{};
ComPtr<ID3D11Buffer>			Renderer::m_MaterialBuffer{};
ComPtr<ID3D11Buffer>			Renderer::m_LightBuffer{};


ComPtr<ID3D11DepthStencilState> Renderer::m_DepthStateEnable{};
ComPtr<ID3D11DepthStencilState> Renderer::m_DepthStateDisable{};


ComPtr<ID3D11BlendState>		Renderer::m_BlendState{};
ComPtr<ID3D11BlendState>		Renderer::m_BlendStateATC{};
ComPtr<ID3D11BlendState>		Renderer::m_BlendStateAdd{}; 
bool Renderer::m_DepthEnabled = true;
RenderBlendMode Renderer::m_BlendMode = RenderBlendMode::Alpha;

namespace
{
	bool CheckInitializationResult(HRESULT result, const char* operation)
	{
		if (SUCCEEDED(result))
		{
			return true;
		}

		char message[256]{};
		sprintf_s(message, "Renderer::Init: %s failed (HRESULT=0x%08X).\n", operation, static_cast<unsigned int>(result));
		OutputDebugStringA(message);
		return false;
	}
}



bool Renderer::Init()
{
	Uninit();
	HRESULT hr = S_OK;




	// デバイス、スワップチェーン作成
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Width = SCREEN_WIDTH;
	swapChainDesc.BufferDesc.Height = SCREEN_HEIGHT;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = GetWindow();
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Windowed = TRUE;

	hr = D3D11CreateDeviceAndSwapChain( NULL,
										D3D_DRIVER_TYPE_HARDWARE,
										NULL,
										0,
										NULL,
										0,
										D3D11_SDK_VERSION,
										&swapChainDesc,
										m_SwapChain.GetAddressOf(),
										m_Device.GetAddressOf(),
										&m_FeatureLevel,
										m_DeviceContext.GetAddressOf() );
	if (!CheckInitializationResult(hr, "D3D11CreateDeviceAndSwapChain"))
	{
		Uninit();
		return false;
	}






	// レンダーターゲットビュー作成
	ComPtr<ID3D11Texture2D> renderTarget{};
	hr = m_SwapChain->GetBuffer(0, IID_PPV_ARGS(renderTarget.ReleaseAndGetAddressOf()));
	if (!CheckInitializationResult(hr, "IDXGISwapChain::GetBuffer"))
	{
		Uninit();
		return false;
	}
	hr = m_Device->CreateRenderTargetView(renderTarget.Get(), nullptr, m_RenderTargetView.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateRenderTargetView"))
	{
		Uninit();
		return false;
	}


	// デプスステンシルバッファ作成
	ComPtr<ID3D11Texture2D> depthStencile{};
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = swapChainDesc.BufferDesc.Width;
	textureDesc.Height = swapChainDesc.BufferDesc.Height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_D16_UNORM;
	textureDesc.SampleDesc = swapChainDesc.SampleDesc;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	hr = m_Device->CreateTexture2D(&textureDesc, nullptr, depthStencile.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateTexture2D(depth)"))
	{
		Uninit();
		return false;
	}

	// デプスステンシルビュー作成
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
	depthStencilViewDesc.Format = textureDesc.Format;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = 0;
	hr = m_Device->CreateDepthStencilView(depthStencile.Get(), &depthStencilViewDesc, m_DepthStencilView.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateDepthStencilView"))
	{
		Uninit();
		return false;
	}


	ID3D11RenderTargetView* renderTargetView = m_RenderTargetView.Get();
	m_DeviceContext->OMSetRenderTargets(1, &renderTargetView, m_DepthStencilView.Get());





	// ビューポート設定
	D3D11_VIEWPORT viewport;
	viewport.Width = (FLOAT)SCREEN_WIDTH;
	viewport.Height = (FLOAT)SCREEN_HEIGHT;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	m_DeviceContext->RSSetViewports( 1, &viewport );



	// ラスタライザステート設定
	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID; 
	rasterizerDesc.CullMode = D3D11_CULL_BACK; 
	rasterizerDesc.DepthClipEnable = TRUE; 
	rasterizerDesc.MultisampleEnable = FALSE; 

	ComPtr<ID3D11RasterizerState> rs;
	hr = m_Device->CreateRasterizerState(&rasterizerDesc, rs.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateRasterizerState"))
	{
		Uninit();
		return false;
	}

	m_DeviceContext->RSSetState( rs.Get() );




	// ブレンドステート設定
	D3D11_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = m_Device->CreateBlendState(&blendDesc, m_BlendState.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBlendState(alpha)"))
	{
		Uninit();
		return false;
	}

	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	hr = m_Device->CreateBlendState(&blendDesc, m_BlendStateAdd.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBlendState(additive)"))
	{
		Uninit();
		return false;
	}

	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.AlphaToCoverageEnable = TRUE;
	hr = m_Device->CreateBlendState(&blendDesc, m_BlendStateATC.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBlendState(alpha-to-coverage)"))
	{
		Uninit();
		return false;
	}

	float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	m_DeviceContext->OMSetBlendState(m_BlendState.Get(), blendFactor, 0xffffffff );





	// デプスステンシルステート設定
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask	= D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	hr = m_Device->CreateDepthStencilState(&depthStencilDesc, m_DepthStateEnable.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateDepthStencilState(enable)"))
	{
		Uninit();
		return false;
	}

	depthStencilDesc.DepthEnable = FALSE;
	depthStencilDesc.DepthWriteMask	= D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = m_Device->CreateDepthStencilState(&depthStencilDesc, m_DepthStateDisable.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateDepthStencilState(disable)"))
	{
		Uninit();
		return false;
	}

	m_DeviceContext->OMSetDepthStencilState( m_DepthStateEnable.Get(), NULL );




	// サンプラーステート設定
	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MaxAnisotropy = 4;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	ComPtr<ID3D11SamplerState> samplerState{};
	hr = m_Device->CreateSamplerState(&samplerDesc, samplerState.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateSamplerState"))
	{
		Uninit();
		return false;
	}

	ID3D11SamplerState* sampler = samplerState.Get();
	m_DeviceContext->PSSetSamplers( 0, 1, &sampler );



	// 定数バッファ生成
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(XMFLOAT4X4);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = sizeof(float);

	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, m_WorldBuffer.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBuffer(world)"))
	{
		Uninit();
		return false;
	}
	ID3D11Buffer* worldBuffer = m_WorldBuffer.Get();
	m_DeviceContext->VSSetConstantBuffers( 0, 1, &worldBuffer);

	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, m_ViewBuffer.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBuffer(view)"))
	{
		Uninit();
		return false;
	}
	ID3D11Buffer* viewBuffer = m_ViewBuffer.Get();
	m_DeviceContext->VSSetConstantBuffers( 1, 1, &viewBuffer );

	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, m_ProjectionBuffer.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBuffer(projection)"))
	{
		Uninit();
		return false;
	}
	ID3D11Buffer* projectionBuffer = m_ProjectionBuffer.Get();
	m_DeviceContext->VSSetConstantBuffers( 2, 1, &projectionBuffer );


	bufferDesc.ByteWidth = sizeof(MATERIAL);

	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, m_MaterialBuffer.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBuffer(material)"))
	{
		Uninit();
		return false;
	}
	ID3D11Buffer* materialBuffer = m_MaterialBuffer.Get();
	m_DeviceContext->VSSetConstantBuffers( 3, 1, &materialBuffer );
	m_DeviceContext->PSSetConstantBuffers( 3, 1, &materialBuffer );


	bufferDesc.ByteWidth = sizeof(LIGHT);

	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, m_LightBuffer.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBuffer(light)"))
	{
		Uninit();
		return false;
	}
	ID3D11Buffer* lightBuffer = m_LightBuffer.Get();
	m_DeviceContext->VSSetConstantBuffers( 4, 1, &lightBuffer );
	m_DeviceContext->PSSetConstantBuffers( 4, 1, &lightBuffer );





	// ライト初期化
	LIGHT light{};
	light.Enable = true;
	light.Direction = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	light.Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.Diffuse = XMFLOAT4(1.5f, 1.5f, 1.5f, 1.0f);
	SetLight(light);



	// マテリアル初期化
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	SetMaterial(material);

	return true;
}



void Renderer::Uninit()
{

	if (m_DeviceContext)
	{
		m_DeviceContext->ClearState();
	}

	m_LightBuffer.Reset();
	m_MaterialBuffer.Reset();
	m_ProjectionBuffer.Reset();
	m_ViewBuffer.Reset();
	m_WorldBuffer.Reset();
	m_BlendStateAdd.Reset();
	m_BlendStateATC.Reset();
	m_BlendState.Reset();
	m_DepthStateDisable.Reset();
	m_DepthStateEnable.Reset();
	m_DepthStencilView.Reset();
	m_RenderTargetView.Reset();
	m_SwapChain.Reset();
	m_DeviceContext.Reset();
	m_Device.Reset();

}




void Renderer::Begin()
{
	SetDepthEnable(true);
	SetBlendMode(RenderBlendMode::Alpha);
	float clearColor[4] = { 0.42f, 0.62f, 0.82f, 1.0f };
	m_DeviceContext->ClearRenderTargetView( m_RenderTargetView.Get(), clearColor );
	m_DeviceContext->ClearDepthStencilView( m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}



void Renderer::End()
{
	m_SwapChain->Present( 1, 0 );
}




void Renderer::SetDepthEnable( bool Enable )
{
	m_DepthEnabled = Enable;
	if( Enable )
		m_DeviceContext->OMSetDepthStencilState( m_DepthStateEnable.Get(), NULL );
	else
		m_DeviceContext->OMSetDepthStencilState( m_DepthStateDisable.Get(), NULL );

}



void Renderer::SetATCEnable( bool Enable )
{
	SetBlendMode(Enable ? RenderBlendMode::AlphaToCoverage : RenderBlendMode::Alpha);
}

void Renderer::SetWorldViewProjection2D()
{
	SetWorldMatrix(XMMatrixIdentity());
	SetViewMatrix(XMMatrixIdentity());

	XMMATRIX projection;
	projection = XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
	SetProjectionMatrix(projection);
}


void Renderer::SetWorldMatrix(XMMATRIX WorldMatrix)
{
	XMFLOAT4X4 worldf;
	XMStoreFloat4x4(&worldf, XMMatrixTranspose(WorldMatrix));
	m_DeviceContext->UpdateSubresource(m_WorldBuffer.Get(), 0, NULL, &worldf, 0, 0);
}

void Renderer::SetViewMatrix(XMMATRIX ViewMatrix)
{
	XMFLOAT4X4 viewf;
	XMStoreFloat4x4(&viewf, XMMatrixTranspose(ViewMatrix));
	m_DeviceContext->UpdateSubresource(m_ViewBuffer.Get(), 0, NULL, &viewf, 0, 0);
}

void Renderer::SetProjectionMatrix(XMMATRIX ProjectionMatrix)
{
	XMFLOAT4X4 projectionf;
	XMStoreFloat4x4(&projectionf, XMMatrixTranspose(ProjectionMatrix));
	m_DeviceContext->UpdateSubresource(m_ProjectionBuffer.Get(), 0, NULL, &projectionf, 0, 0);

}



void Renderer::SetMaterial( MATERIAL Material )
{
	m_DeviceContext->UpdateSubresource( m_MaterialBuffer.Get(), 0, NULL, &Material, 0, 0 );
}

void Renderer::SetLight( LIGHT Light )
{
	m_DeviceContext->UpdateSubresource(m_LightBuffer.Get(), 0, NULL, &Light, 0, 0);
}

void Renderer::SetAddBlendEnable(bool Enable)
{
	SetBlendMode(Enable ? RenderBlendMode::Additive : RenderBlendMode::Alpha);
}

void Renderer::SetBlendMode(RenderBlendMode mode)
{
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_BlendMode = mode;
	if (mode == RenderBlendMode::Additive)
	{
		m_DeviceContext->OMSetBlendState(m_BlendStateAdd.Get(), blendFactor, 0xffffffff);
	}
	else if (mode == RenderBlendMode::AlphaToCoverage)
	{
		m_DeviceContext->OMSetBlendState(m_BlendStateATC.Get(), blendFactor, 0xffffffff);
	}
	else
	{
		m_DeviceContext->OMSetBlendState(m_BlendState.Get(), blendFactor, 0xffffffff);
	}
}

struct RendererStateScope::Snapshot
{
	static constexpr UINT ConstantBufferCount = 5;

	ComPtr<ID3D11InputLayout> InputLayout;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	ComPtr<ID3D11Buffer> VertexBuffer;
	UINT VertexStride = 0;
	UINT VertexOffset = 0;
	ComPtr<ID3D11Buffer> IndexBuffer;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_UNKNOWN;
	UINT IndexOffset = 0;
	ComPtr<ID3D11VertexShader> VertexShader;
	ComPtr<ID3D11PixelShader> PixelShader;
	std::array<ComPtr<ID3D11Buffer>, ConstantBufferCount> VSConstantBuffers;
	std::array<ComPtr<ID3D11Buffer>, ConstantBufferCount> PSConstantBuffers;
	ComPtr<ID3D11RasterizerState> RasterizerState;
	std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> Viewports{};
	UINT ViewportCount = static_cast<UINT>(Viewports.size());
	std::array<D3D11_RECT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> ScissorRects{};
	UINT ScissorRectCount = static_cast<UINT>(ScissorRects.size());
	ComPtr<ID3D11DepthStencilState> DepthStencilState;
	UINT StencilReference = 0;
	ComPtr<ID3D11BlendState> BlendState;
	std::array<float, 4> BlendFactor{};
	UINT SampleMask = 0xffffffff;
	ComPtr<ID3D11RenderTargetView> RenderTarget;
	ComPtr<ID3D11DepthStencilView> DepthStencilView;
	ComPtr<ID3D11SamplerState> PixelSampler;
	ComPtr<ID3D11ShaderResourceView> PixelResource;
};

RendererStateScope::RendererStateScope()
	: m_Snapshot(std::make_unique<Snapshot>()),
	m_DepthEnabled(Renderer::GetDepthEnabled()),
	m_BlendMode(Renderer::GetBlendMode())
{
	ID3D11DeviceContext* context = Renderer::GetDeviceContext();
	if (context == nullptr)
	{
		m_Snapshot.reset();
		return;
	}

	ID3D11InputLayout* inputLayout = nullptr;
	context->IAGetInputLayout(&inputLayout);
	m_Snapshot->InputLayout.Attach(inputLayout);
	context->IAGetPrimitiveTopology(&m_Snapshot->Topology);
	ID3D11Buffer* vertexBuffer = nullptr;
	context->IAGetVertexBuffers(0, 1, &vertexBuffer, &m_Snapshot->VertexStride, &m_Snapshot->VertexOffset);
	m_Snapshot->VertexBuffer.Attach(vertexBuffer);
	ID3D11Buffer* indexBuffer = nullptr;
	context->IAGetIndexBuffer(&indexBuffer, &m_Snapshot->IndexFormat, &m_Snapshot->IndexOffset);
	m_Snapshot->IndexBuffer.Attach(indexBuffer);

	ID3D11VertexShader* vertexShader = nullptr;
	context->VSGetShader(&vertexShader, nullptr, nullptr);
	m_Snapshot->VertexShader.Attach(vertexShader);
	ID3D11PixelShader* pixelShader = nullptr;
	context->PSGetShader(&pixelShader, nullptr, nullptr);
	m_Snapshot->PixelShader.Attach(pixelShader);

	std::array<ID3D11Buffer*, Snapshot::ConstantBufferCount> vsBuffers{};
	std::array<ID3D11Buffer*, Snapshot::ConstantBufferCount> psBuffers{};
	context->VSGetConstantBuffers(0, Snapshot::ConstantBufferCount, vsBuffers.data());
	context->PSGetConstantBuffers(0, Snapshot::ConstantBufferCount, psBuffers.data());
	for (UINT index = 0; index < Snapshot::ConstantBufferCount; ++index)
	{
		m_Snapshot->VSConstantBuffers[index].Attach(vsBuffers[index]);
		m_Snapshot->PSConstantBuffers[index].Attach(psBuffers[index]);
	}

	ID3D11RasterizerState* rasterizerState = nullptr;
	context->RSGetState(&rasterizerState);
	m_Snapshot->RasterizerState.Attach(rasterizerState);
	context->RSGetViewports(&m_Snapshot->ViewportCount, m_Snapshot->Viewports.data());
	context->RSGetScissorRects(&m_Snapshot->ScissorRectCount, m_Snapshot->ScissorRects.data());

	ID3D11DepthStencilState* depthStencilState = nullptr;
	context->OMGetDepthStencilState(&depthStencilState, &m_Snapshot->StencilReference);
	m_Snapshot->DepthStencilState.Attach(depthStencilState);
	ID3D11BlendState* blendState = nullptr;
	context->OMGetBlendState(&blendState, m_Snapshot->BlendFactor.data(), &m_Snapshot->SampleMask);
	m_Snapshot->BlendState.Attach(blendState);
	ID3D11RenderTargetView* renderTarget = nullptr;
	ID3D11DepthStencilView* depthStencilView = nullptr;
	context->OMGetRenderTargets(1, &renderTarget, &depthStencilView);
	m_Snapshot->RenderTarget.Attach(renderTarget);
	m_Snapshot->DepthStencilView.Attach(depthStencilView);

	ID3D11SamplerState* pixelSampler = nullptr;
	context->PSGetSamplers(0, 1, &pixelSampler);
	m_Snapshot->PixelSampler.Attach(pixelSampler);
	ID3D11ShaderResourceView* pixelResource = nullptr;
	context->PSGetShaderResources(0, 1, &pixelResource);
	m_Snapshot->PixelResource.Attach(pixelResource);
}

RendererStateScope::~RendererStateScope()
{
	Restore();
}

void RendererStateScope::Restore() const
{
	ID3D11DeviceContext* context = Renderer::GetDeviceContext();
	if (context == nullptr || m_Snapshot == nullptr)
	{
		return;
	}

	Renderer::SetDepthEnable(m_DepthEnabled);
	Renderer::SetBlendMode(m_BlendMode);
	context->IASetInputLayout(m_Snapshot->InputLayout.Get());
	context->IASetPrimitiveTopology(m_Snapshot->Topology);
	ID3D11Buffer* vertexBuffer = m_Snapshot->VertexBuffer.Get();
	context->IASetVertexBuffers(0, 1, &vertexBuffer, &m_Snapshot->VertexStride, &m_Snapshot->VertexOffset);
	context->IASetIndexBuffer(m_Snapshot->IndexBuffer.Get(), m_Snapshot->IndexFormat, m_Snapshot->IndexOffset);
	context->VSSetShader(m_Snapshot->VertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_Snapshot->PixelShader.Get(), nullptr, 0);

	std::array<ID3D11Buffer*, Snapshot::ConstantBufferCount> vsBuffers{};
	std::array<ID3D11Buffer*, Snapshot::ConstantBufferCount> psBuffers{};
	for (UINT index = 0; index < Snapshot::ConstantBufferCount; ++index)
	{
		vsBuffers[index] = m_Snapshot->VSConstantBuffers[index].Get();
		psBuffers[index] = m_Snapshot->PSConstantBuffers[index].Get();
	}
	context->VSSetConstantBuffers(0, Snapshot::ConstantBufferCount, vsBuffers.data());
	context->PSSetConstantBuffers(0, Snapshot::ConstantBufferCount, psBuffers.data());
	context->RSSetState(m_Snapshot->RasterizerState.Get());
	context->RSSetViewports(m_Snapshot->ViewportCount, m_Snapshot->Viewports.data());
	context->RSSetScissorRects(m_Snapshot->ScissorRectCount, m_Snapshot->ScissorRects.data());
	context->OMSetDepthStencilState(m_Snapshot->DepthStencilState.Get(), m_Snapshot->StencilReference);
	context->OMSetBlendState(m_Snapshot->BlendState.Get(), m_Snapshot->BlendFactor.data(), m_Snapshot->SampleMask);
	ID3D11RenderTargetView* renderTarget = m_Snapshot->RenderTarget.Get();
	context->OMSetRenderTargets(1, &renderTarget, m_Snapshot->DepthStencilView.Get());
	ID3D11SamplerState* pixelSampler = m_Snapshot->PixelSampler.Get();
	context->PSSetSamplers(0, 1, &pixelSampler);
	ID3D11ShaderResourceView* pixelResource = m_Snapshot->PixelResource.Get();
	context->PSSetShaderResources(0, 1, &pixelResource);
}





bool Renderer::CreateVertexShader( ComPtr<ID3D11VertexShader>& VertexShader, ComPtr<ID3D11InputLayout>& VertexLayout, const char* FileName )
{
	VertexShader.Reset();
	VertexLayout.Reset();
	if (FileName == nullptr)
	{
		return false;
	}

	const VertexShaderHandle resource = ResourceManager::LoadVertexShader(FileName);
	if (resource != nullptr)
	{
		VertexShader = resource->Shader;
		VertexLayout = resource->InputLayout;
	}
	return VertexShader != nullptr && VertexLayout != nullptr;
}



bool Renderer::CreatePixelShader( ComPtr<ID3D11PixelShader>& PixelShader, const char* FileName )
{
	PixelShader.Reset();
	if (FileName == nullptr)
	{
		return false;
	}

	const PixelShaderHandle resource = ResourceManager::LoadPixelShader(FileName);
	if (resource != nullptr)
	{
		PixelShader = resource->Shader;
	}
	return PixelShader != nullptr;
}

}


