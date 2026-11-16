
#include "main.h"
#include "renderer.h"
#include "resourceManager.h"
#include "shaderCatalog.h"
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
ComPtr<ID3D11Buffer>			Renderer::m_CameraBuffer{};
ComPtr<ID3D11Buffer>			Renderer::m_ParameterBuffer{};
ComPtr<ID3D11Buffer>			Renderer::m_PostOptionsBuffer{};
ComPtr<ID3D11Texture2D>		Renderer::m_SceneTexture{};
ComPtr<ID3D11RenderTargetView> Renderer::m_SceneRenderTarget{};
ComPtr<ID3D11ShaderResourceView> Renderer::m_SceneResource{};
ComPtr<ID3D11Buffer>			Renderer::m_PostVertexBuffer{};
std::array<ComPtr<ID3D11Texture2D>, Renderer::PostWorkSlots> Renderer::m_PostWorkTextures{};
std::array<ComPtr<ID3D11RenderTargetView>, Renderer::PostWorkSlots> Renderer::m_PostWorkTargets{};
std::array<ComPtr<ID3D11ShaderResourceView>, Renderer::PostWorkSlots> Renderer::m_PostWorkResources{};
PostEffectMode Renderer::m_PostEffectMode = PostEffectMode::None;
std::string Renderer::m_PostDefinitionId{};
void (*Renderer::m_OverlayCallback)() = nullptr;
XMFLOAT4 Renderer::m_PostOptions{};
XMFLOAT4 Renderer::m_PostExtraOptions{};
bool Renderer::m_CustomPostOptions = false;
XMFLOAT4 Renderer::m_PostDefinitionParameters{};
XMFLOAT4 Renderer::m_ClearColor = Renderer::DefaultClearColor;


ComPtr<ID3D11DepthStencilState> Renderer::m_DepthStateEnable{};
ComPtr<ID3D11DepthStencilState> Renderer::m_DepthStateTestOnly{};
ComPtr<ID3D11DepthStencilState> Renderer::m_DepthStateDisable{};


ComPtr<ID3D11BlendState>		Renderer::m_BlendState{};
ComPtr<ID3D11BlendState>		Renderer::m_BlendStateATC{};
ComPtr<ID3D11BlendState>		Renderer::m_BlendStateAdd{}; 
ComPtr<ID3D11RasterizerState> Renderer::m_RasterStateBack{};
ComPtr<ID3D11RasterizerState> Renderer::m_RasterStateFront{};
RenderDepthMode Renderer::m_DepthMode = RenderDepthMode::Enabled;
RenderBlendMode Renderer::m_BlendMode = RenderBlendMode::Alpha;
LIGHT Renderer::m_Light{};

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

	hr = m_Device->CreateRasterizerState(&rasterizerDesc, m_RasterStateBack.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateRasterizerState"))
	{
		Uninit();
		return false;
	}

	rasterizerDesc.CullMode = D3D11_CULL_FRONT;
	hr = m_Device->CreateRasterizerState(&rasterizerDesc, m_RasterStateFront.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateRasterizerState(front)"))
	{
		Uninit();
		return false;
	}
	m_DeviceContext->RSSetState(m_RasterStateBack.Get());




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

	// 深度テストのみ。加算合成のエフェクトが壁に隠れつつ、深度を汚さないための状態です。
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	hr = m_Device->CreateDepthStencilState(&depthStencilDesc, m_DepthStateTestOnly.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateDepthStencilState(test only)"))
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
	VERTEX_3D postVertices[3]{};
	postVertices[0].Position = { -1.0f, -1.0f, 0.0f };
	postVertices[0].TexCoord = { 0.0f, 1.0f };
	postVertices[1].Position = { -1.0f, 3.0f, 0.0f };
	postVertices[1].TexCoord = { 0.0f, -1.0f };
	postVertices[2].Position = { 3.0f, -1.0f, 0.0f };
	postVertices[2].TexCoord = { 2.0f, 1.0f };
	D3D11_BUFFER_DESC postDesc{};
	postDesc.ByteWidth = sizeof(postVertices);
	postDesc.Usage = D3D11_USAGE_IMMUTABLE;
	postDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA postData{};
	postData.pSysMem = postVertices;
	hr = m_Device->CreateBuffer(&postDesc, &postData, m_PostVertexBuffer.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBuffer(post vertices)")) { Uninit(); return false; }



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

	bufferDesc.ByteWidth = sizeof(XMFLOAT4);
	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, m_CameraBuffer.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBuffer(camera)"))
	{
		Uninit();
		return false;
	}
	D3D11_TEXTURE2D_DESC sceneDesc{};
	sceneDesc.Width = SCREEN_WIDTH;
	sceneDesc.Height = SCREEN_HEIGHT;
	sceneDesc.MipLevels = 0;
	sceneDesc.ArraySize = 1;
	sceneDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sceneDesc.SampleDesc.Count = 1;
	sceneDesc.Usage = D3D11_USAGE_DEFAULT;
	sceneDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	sceneDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	hr = m_Device->CreateTexture2D(&sceneDesc, nullptr, m_SceneTexture.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateTexture2D(scene)")) { Uninit(); return false; }
	hr = m_Device->CreateRenderTargetView(m_SceneTexture.Get(), nullptr, m_SceneRenderTarget.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateRenderTargetView(scene)")) { Uninit(); return false; }
	hr = m_Device->CreateShaderResourceView(m_SceneTexture.Get(), nullptr, m_SceneResource.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateShaderResourceView(scene)")) { Uninit(); return false; }
	D3D11_TEXTURE2D_DESC workDesc = sceneDesc;
	workDesc.MipLevels = 1;
	workDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	workDesc.MiscFlags = 0;
	for (std::size_t i = 0; i < m_PostWorkTextures.size(); ++i)
	{
		workDesc.Width = SCREEN_WIDTH >> PostWorkSlotShift(i);
		workDesc.Height = SCREEN_HEIGHT >> PostWorkSlotShift(i);
		hr = m_Device->CreateTexture2D(&workDesc, nullptr, m_PostWorkTextures[i].ReleaseAndGetAddressOf());
		if (!CheckInitializationResult(hr, "ID3D11Device::CreateTexture2D(post work)")) { Uninit(); return false; }
		hr = m_Device->CreateRenderTargetView(m_PostWorkTextures[i].Get(), nullptr, m_PostWorkTargets[i].ReleaseAndGetAddressOf());
		if (!CheckInitializationResult(hr, "ID3D11Device::CreateRenderTargetView(post work)")) { Uninit(); return false; }
		hr = m_Device->CreateShaderResourceView(m_PostWorkTextures[i].Get(), nullptr, m_PostWorkResources[i].ReleaseAndGetAddressOf());
		if (!CheckInitializationResult(hr, "ID3D11Device::CreateShaderResourceView(post work)")) { Uninit(); return false; }
	}
	ID3D11Buffer* cameraBuffer = m_CameraBuffer.Get();
	m_DeviceContext->VSSetConstantBuffers(5, 1, &cameraBuffer);
	m_DeviceContext->PSSetConstantBuffers(5, 1, &cameraBuffer);

	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, m_ParameterBuffer.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBuffer(parameter)"))
	{
		Uninit();
		return false;
	}
	ID3D11Buffer* parameterBuffer = m_ParameterBuffer.Get();
	m_DeviceContext->VSSetConstantBuffers(6, 1, &parameterBuffer);
	m_DeviceContext->PSSetConstantBuffers(6, 1, &parameterBuffer);
	hr = m_Device->CreateBuffer(&bufferDesc, nullptr, m_PostOptionsBuffer.ReleaseAndGetAddressOf());
	if (!CheckInitializationResult(hr, "ID3D11Device::CreateBuffer(post options)"))
	{
		Uninit();
		return false;
	}





	// ライト初期化
	LIGHT light{};
	light.Enable = true;
	light.Direction = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	light.Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.Diffuse = XMFLOAT4(1.5f, 1.5f, 1.5f, 1.0f);
	SetLight(light);
	SetCameraPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));
	SetParameter(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));



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
	m_PostVertexBuffer.Reset();
	for (auto& resource : m_PostWorkResources) resource.Reset();
	for (auto& target : m_PostWorkTargets) target.Reset();
	for (auto& texture : m_PostWorkTextures) texture.Reset();
	m_SceneResource.Reset();
	m_SceneRenderTarget.Reset();
	m_SceneTexture.Reset();
	m_PostEffectMode = PostEffectMode::None;
	m_PostDefinitionId.clear();
	m_ParameterBuffer.Reset();
	m_PostOptionsBuffer.Reset();
	m_CustomPostOptions = false;
	m_CameraBuffer.Reset();
	m_MaterialBuffer.Reset();
	m_ProjectionBuffer.Reset();
	m_ViewBuffer.Reset();
	m_WorldBuffer.Reset();
	m_BlendStateAdd.Reset();
	m_BlendStateATC.Reset();
	m_BlendState.Reset();
	m_RasterStateFront.Reset();
	m_RasterStateBack.Reset();
	m_DepthStateDisable.Reset();
	m_DepthStateTestOnly.Reset();
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
	ID3D11RenderTargetView* target = m_PostEffectMode == PostEffectMode::None
		&& m_PostDefinitionId.empty()
		? m_RenderTargetView.Get() : m_SceneRenderTarget.Get();
	m_DeviceContext->OMSetRenderTargets(1, &target, m_DepthStencilView.Get());
	const float clearColor[4] = { m_ClearColor.x, m_ClearColor.y, m_ClearColor.z, m_ClearColor.w };
	m_DeviceContext->ClearRenderTargetView(target, clearColor);
	m_DeviceContext->ClearDepthStencilView( m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}



void Renderer::ClearDepth()
{
	m_DeviceContext->ClearDepthStencilView(m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}



void Renderer::End()
{
	if (m_PostEffectMode != PostEffectMode::None || !m_PostDefinitionId.empty())
	{
		const ShaderDefinition* definition = m_PostDefinitionId.empty()
			? nullptr : FindShaderDefinition(m_PostDefinitionId);
		if (definition && definition->Passes.size() > 1)
		{
			ID3D11RenderTargetView* backBuffer = m_RenderTargetView.Get();
			m_DeviceContext->OMSetRenderTargets(1, &backBuffer, nullptr);
			if (definition->GenerateMips) m_DeviceContext->GenerateMips(m_SceneResource.Get());
			DrawPostDefinition(*definition);
			if (m_OverlayCallback != nullptr) m_OverlayCallback();
			m_SwapChain->Present(1, 0);
			return;
		}
		const VertexShaderHandle vs = ResourceManager::LoadVertexShader(definition
			? std::string(definition->Passes[0].VertexFile) : "spPostVS.cso");
		const PixelShaderHandle ps = ResourceManager::LoadPixelShader(definition
			? std::string(definition->Passes[0].PixelFile) : "spPostPS.cso");
		ID3D11RenderTargetView* backBuffer = m_RenderTargetView.Get();
		m_DeviceContext->OMSetRenderTargets(1, &backBuffer, nullptr);
		if (m_PostEffectMode == PostEffectMode::MipPreview
			|| (definition && definition->GenerateMips))
			m_DeviceContext->GenerateMips(m_SceneResource.Get());
		if (vs && ps)
		{
			SetDepthMode(RenderDepthMode::Disabled);
			SetParameter(definition ? m_PostDefinitionParameters
				: XMFLOAT4(static_cast<float>(static_cast<int>(m_PostEffectMode)),
					m_CustomPostOptions ? m_PostOptions.x : 12.0f,
					m_CustomPostOptions ? m_PostOptions.y : 0.18f,
					m_CustomPostOptions ? m_PostOptions.z : 0.0f));
			XMFLOAT4 extra = m_CustomPostOptions ? m_PostExtraOptions : XMFLOAT4{};
			extra.z = m_CustomPostOptions ? 1.0f : 0.0f;
			m_DeviceContext->UpdateSubresource(m_PostOptionsBuffer.Get(), 0, nullptr, &extra, 0, 0);
			ID3D11Buffer* postOptions = m_PostOptionsBuffer.Get();
			m_DeviceContext->PSSetConstantBuffers(7, 1, &postOptions);
			m_DeviceContext->IASetInputLayout(vs->InputLayout.Get());
			m_DeviceContext->VSSetShader(vs->Shader.Get(), nullptr, 0);
			m_DeviceContext->PSSetShader(ps->Shader.Get(), nullptr, 0);
			ID3D11ShaderResourceView* scene = m_SceneResource.Get();
			m_DeviceContext->PSSetShaderResources(0, 1, &scene);
			UINT stride = sizeof(VERTEX_3D), offset = 0;
			m_DeviceContext->IASetVertexBuffers(0, 1, m_PostVertexBuffer.GetAddressOf(), &stride, &offset);
			m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_DeviceContext->Draw(3, 0);
			ID3D11ShaderResourceView* empty = nullptr;
			m_DeviceContext->PSSetShaderResources(0, 1, &empty);
		}
		else
		{
			ComPtr<ID3D11Resource> backResource;
			m_RenderTargetView->GetResource(backResource.GetAddressOf());
			m_DeviceContext->CopyResource(backResource.Get(), m_SceneTexture.Get());
		}
	}
	if (m_OverlayCallback != nullptr) m_OverlayCallback();
	m_SwapChain->Present( 1, 0 );
}

bool Renderer::SetPostEffectDefinition(std::string_view id)
{
	const ShaderDefinition* definition = FindShaderDefinition(id);
	if (!definition || definition->Usage != ShaderUsage::Post
		|| definition->Passes.empty() || definition->Passes.size() > MaxPostPasses)
		return false;
	for (const ShaderPass& pass : definition->Passes)
		if (!ResourceManager::LoadVertexShader(std::string(pass.VertexFile))
			|| !ResourceManager::LoadPixelShader(std::string(pass.PixelFile)))
			return false;
	m_PostDefinitionId = id;
	m_PostDefinitionParameters = definition->Defaults;
	m_PostEffectMode = PostEffectMode::None;
	return true;
}

// 最後のパス以外は作業バッファの枠(ShaderPass::PostOutput、未指定なら 1/4 の2枠を交互)へ描く。
// 各パスは t0=元画面、t1=直前のパスの出力、t2〜t7=作業枠0〜5、b6=名前付き値、b7=パス固有値を受け取る。
void Renderer::DrawPostDefinition(const ShaderDefinition& definition)
{
	std::array<VertexShaderHandle, MaxPostPasses> vertexShaders{};
	std::array<PixelShaderHandle, MaxPostPasses> pixelShaders{};
	for (std::size_t i = 0; i < definition.Passes.size(); ++i)
	{
		vertexShaders[i] = ResourceManager::LoadVertexShader(std::string(definition.Passes[i].VertexFile));
		pixelShaders[i] = ResourceManager::LoadPixelShader(std::string(definition.Passes[i].PixelFile));
		if (!vertexShaders[i] || !pixelShaders[i])
		{
			ComPtr<ID3D11Resource> backResource;
			m_RenderTargetView->GetResource(backResource.GetAddressOf());
			m_DeviceContext->CopyResource(backResource.Get(), m_SceneTexture.Get());
			return;
		}
	}

	UINT viewportCount = 1;
	D3D11_VIEWPORT fullViewport{};
	m_DeviceContext->RSGetViewports(&viewportCount, &fullViewport);

	SetDepthMode(RenderDepthMode::Disabled);
	SetParameter(m_PostDefinitionParameters);
	ID3D11Buffer* passBuffer = m_PostOptionsBuffer.Get();
	m_DeviceContext->PSSetConstantBuffers(7, 1, &passBuffer);
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	m_DeviceContext->IASetVertexBuffers(0, 1, m_PostVertexBuffer.GetAddressOf(), &stride, &offset);
	m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	constexpr UINT inputCount = 2 + static_cast<UINT>(PostWorkSlots);
	ID3D11RenderTargetView* backBuffer = m_RenderTargetView.Get();
	ID3D11ShaderResourceView* previous = m_SceneResource.Get();
	ID3D11ShaderResourceView* empty[inputCount]{};
	for (std::size_t i = 0; i < definition.Passes.size(); ++i)
	{
		const ShaderPass& pass = definition.Passes[i];
		const bool last = i + 1 == definition.Passes.size();
		const std::size_t slot = pass.PostOutput >= 0 && pass.PostOutput < static_cast<int>(PostWorkSlots)
			? static_cast<std::size_t>(pass.PostOutput) : 2 + i % 2;
		m_DeviceContext->PSSetShaderResources(0, inputCount, empty);
		ID3D11RenderTargetView* target = last ? backBuffer : m_PostWorkTargets[slot].Get();
		m_DeviceContext->OMSetRenderTargets(1, &target, nullptr);
		D3D11_VIEWPORT viewport = fullViewport;
		if (!last)
		{
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = static_cast<FLOAT>(SCREEN_WIDTH >> PostWorkSlotShift(slot));
			viewport.Height = static_cast<FLOAT>(SCREEN_HEIGHT >> PostWorkSlotShift(slot));
		}
		m_DeviceContext->RSSetViewports(1, &viewport);
		m_DeviceContext->UpdateSubresource(m_PostOptionsBuffer.Get(), 0, nullptr, &pass.Parameters, 0, 0);
		m_DeviceContext->IASetInputLayout(vertexShaders[i]->InputLayout.Get());
		m_DeviceContext->VSSetShader(vertexShaders[i]->Shader.Get(), nullptr, 0);
		m_DeviceContext->PSSetShader(pixelShaders[i]->Shader.Get(), nullptr, 0);
		// t2〜t7 には全作業枠を渡す(このパスが書く枠だけは空にする)。
		ID3D11ShaderResourceView* inputs[inputCount]{ m_SceneResource.Get(), previous };
		for (std::size_t s = 0; s < PostWorkSlots; ++s)
			inputs[2 + s] = !last && s == slot ? nullptr : m_PostWorkResources[s].Get();
		if (!last && previous == m_PostWorkResources[slot].Get()) inputs[1] = nullptr;
		m_DeviceContext->PSSetShaderResources(0, inputCount, inputs);
		m_DeviceContext->Draw(3, 0);
		previous = m_PostWorkResources[slot].Get();
	}
	m_DeviceContext->PSSetShaderResources(0, inputCount, empty);
}

bool Renderer::SetPostEffectFloat(std::string_view name, float value)
{
	const ShaderDefinition* definition = FindShaderDefinition(m_PostDefinitionId);
	if (!definition || definition->Usage != ShaderUsage::Post || name.empty()) return false;
	float* parameters = &m_PostDefinitionParameters.x;
	for (std::size_t i = 0; i < definition->ParameterNames.size(); ++i)
		if (definition->ParameterNames[i] == name)
		{
			parameters[i] = value;
			return true;
		}
	return false;
}




void Renderer::SetDepthMode( RenderDepthMode Mode )
{
	m_DepthMode = Mode;
	switch( Mode )
	{
	case RenderDepthMode::TestOnly:
		m_DeviceContext->OMSetDepthStencilState( m_DepthStateTestOnly.Get(), NULL );
		break;
	case RenderDepthMode::Disabled:
		m_DeviceContext->OMSetDepthStencilState( m_DepthStateDisable.Get(), NULL );
		break;
	default:
		m_DeviceContext->OMSetDepthStencilState( m_DepthStateEnable.Get(), NULL );
		break;
	}

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
	m_Light = Light;
	m_DeviceContext->UpdateSubresource(m_LightBuffer.Get(), 0, NULL, &Light, 0, 0);
}

void Renderer::SetCameraPosition(XMFLOAT3 CameraPosition)
{
	const XMFLOAT4 value(CameraPosition.x, CameraPosition.y, CameraPosition.z, 1.0f);
	m_DeviceContext->UpdateSubresource(m_CameraBuffer.Get(), 0, NULL, &value, 0, 0);
}

void Renderer::SetParameter(XMFLOAT4 Parameter)
{
	m_DeviceContext->UpdateSubresource(m_ParameterBuffer.Get(), 0, NULL, &Parameter, 0, 0);
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

void Renderer::SetCullFront(bool enabled)
{
	m_DeviceContext->RSSetState(enabled ? m_RasterStateFront.Get() : m_RasterStateBack.Get());
}

struct RendererStateScope::Snapshot
{
	static constexpr UINT ConstantBufferCount = 7;

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
	std::array<ComPtr<ID3D11ShaderResourceView>, 2> PixelResources;
};

RendererStateScope::RendererStateScope()
	: m_Snapshot(std::make_unique<Snapshot>()),
	m_DepthMode(Renderer::GetDepthMode()),
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
	ID3D11ShaderResourceView* pixelResources[2]{};
	context->PSGetShaderResources(0, 2, pixelResources);
	for (UINT index = 0; index < 2; ++index)
		m_Snapshot->PixelResources[index].Attach(pixelResources[index]);
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

	Renderer::SetDepthMode(m_DepthMode);
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
	ID3D11ShaderResourceView* pixelResources[2] = {
		m_Snapshot->PixelResources[0].Get(), m_Snapshot->PixelResources[1].Get() };
	context->PSSetShaderResources(0, 2, pixelResources);
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


