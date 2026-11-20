#include "shadowMap.h"

#include "renderer.h"
#include "resourceManager.h"

#include <cmath>

namespace Engine
{
namespace
{
	// Shadow.hlsli の ShadowBuffer と同じ並びです。
	struct ShadowConstants
	{
		XMFLOAT4X4 LightViewProjection;
		// x=1テクセルの大きさ[m], y=深度の偏り, z=有効(1)/無効(0), w=1テクセルのUV幅
		XMFLOAT4 Parameters;
	};

	// 既存の RendererStateScope が触らないスロットを使います(定数b0-b6, サンプラーs0, テクスチャt0-t1)。
	constexpr UINT kConstantSlot = 8;
	constexpr UINT kTextureSlot = 4;
	constexpr UINT kSamplerSlot = 1;
	// 焦点の手前・奥に取る奥行き[m]です。地形全体と木の高さが収まる大きさにします。
	constexpr float kDepthRange = 80.0f;
	// 影の受け手が自分の影で縞になる(シャドウアクネ)のを防ぐ、奥行き方向の偏り[m]です。
	constexpr float kDepthBiasMeters = 0.02f;

	ComPtr<ID3D11Texture2D> g_Texture;
	ComPtr<ID3D11DepthStencilView> g_DepthView;
	ComPtr<ID3D11ShaderResourceView> g_ShaderView;
	ComPtr<ID3D11SamplerState> g_Sampler;
	ComPtr<ID3D11RasterizerState> g_Rasterizer;
	ComPtr<ID3D11Buffer> g_Constants;
	VertexShaderHandle g_CasterVertexShader;
	PixelShaderHandle g_CasterPixelShader;

	bool Check(HRESULT result, const char* operation)
	{
		if (SUCCEEDED(result)) return true;
		char message[256]{};
		sprintf_s(message, "ShadowMap: %s failed (HRESULT=0x%08X).\n", operation, static_cast<unsigned int>(result));
		OutputDebugStringA(message);
		return false;
	}
}

bool ShadowMap::s_Rendering = false;

bool ShadowMap::EnsureResources()
{
	if (g_Texture)
	{
		return true;
	}
	ID3D11Device* device = Renderer::GetDevice();
	if (device == nullptr)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC textureDescription{};
	textureDescription.Width = kResolution;
	textureDescription.Height = kResolution;
	textureDescription.MipLevels = 1;
	textureDescription.ArraySize = 1;
	// 深度として書き、影の判定ではテクスチャとして読むため、型なしで作ります。
	textureDescription.Format = DXGI_FORMAT_R32_TYPELESS;
	textureDescription.SampleDesc.Count = 1;
	textureDescription.Usage = D3D11_USAGE_DEFAULT;
	textureDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	if (!Check(device->CreateTexture2D(&textureDescription, nullptr, g_Texture.GetAddressOf()), "CreateTexture2D"))
	{
		Release();
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC depthDescription{};
	depthDescription.Format = DXGI_FORMAT_D32_FLOAT;
	depthDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	D3D11_SHADER_RESOURCE_VIEW_DESC shaderDescription{};
	shaderDescription.Format = DXGI_FORMAT_R32_FLOAT;
	shaderDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderDescription.Texture2D.MipLevels = 1;

	// 比較サンプラーは周囲4テクセルとの比較結果を補間するので、影の縁が階段状になりにくくなります。
	// 範囲外は日なた扱いにするため、境界色を1(最も遠い深度)にします。
	D3D11_SAMPLER_DESC samplerDescription{};
	samplerDescription.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDescription.BorderColor[0] = samplerDescription.BorderColor[1] = 1.0f;
	samplerDescription.BorderColor[2] = samplerDescription.BorderColor[3] = 1.0f;
	samplerDescription.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;

	// 布や板の木は裏表どちらからも影を落とすので、カリングしません。
	// 斜面ほど深度をずらし、受け手側の偏りと合わせてアクネを抑えます。
	D3D11_RASTERIZER_DESC rasterizerDescription{};
	rasterizerDescription.FillMode = D3D11_FILL_SOLID;
	rasterizerDescription.CullMode = D3D11_CULL_NONE;
	rasterizerDescription.DepthClipEnable = TRUE;
	rasterizerDescription.SlopeScaledDepthBias = 1.5f;

	D3D11_BUFFER_DESC constantDescription{};
	constantDescription.ByteWidth = sizeof(ShadowConstants);
	constantDescription.Usage = D3D11_USAGE_DEFAULT;
	constantDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if (!Check(device->CreateDepthStencilView(g_Texture.Get(), &depthDescription, g_DepthView.GetAddressOf()), "CreateDepthStencilView")
		|| !Check(device->CreateShaderResourceView(g_Texture.Get(), &shaderDescription, g_ShaderView.GetAddressOf()), "CreateShaderResourceView")
		|| !Check(device->CreateSamplerState(&samplerDescription, g_Sampler.GetAddressOf()), "CreateSamplerState")
		|| !Check(device->CreateRasterizerState(&rasterizerDescription, g_Rasterizer.GetAddressOf()), "CreateRasterizerState")
		|| !Check(device->CreateBuffer(&constantDescription, nullptr, g_Constants.GetAddressOf()), "CreateBuffer"))
	{
		Release();
		return false;
	}
	return true;
}

Vector3 ShadowMap::GetLightDirection()
{
	const LIGHT& light = Renderer::GetLight();
	Vector3 direction{ light.Direction.x, light.Direction.y, light.Direction.z };
	// NaN・無限大は長さの比較をすり抜けて行列全体を壊すので、長さが足りないときと同じく真下にします。
	const float length = direction.length();
	if (!std::isfinite(length) || length < 1e-4f)
	{
		return { 0.0f, -1.0f, 0.0f };
	}
	direction.normalize();
	return direction;
}

bool ShadowMap::Begin(const Vector3& focus)
{
	if (!std::isfinite(focus.x) || !std::isfinite(focus.y) || !std::isfinite(focus.z))
	{
		return false;
	}
	ID3D11DeviceContext* context = Renderer::GetDeviceContext();
	if (context == nullptr || !EnsureResources())
	{
		return false;
	}

	// 前フレームで受け手用にテクスチャとして繋いだままだと、深度として書けません。
	ID3D11ShaderResourceView* nullView = nullptr;
	context->PSSetShaderResources(kTextureSlot, 1, &nullView);

	// 太陽の向きから見た正射影です。焦点を1テクセル単位に揃えると、カメラが動いても影の縁がちらつきません。
	const Vector3 direction = GetLightDirection();
	const XMVECTOR up = fabsf(direction.y) > 0.99f
		? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMMATRIX view = XMMatrixLookToLH(XMVectorZero(),
		XMVectorSet(direction.x, direction.y, direction.z, 0.0f), up);
	XMFLOAT3 focusInLight;
	XMStoreFloat3(&focusInLight, XMVector3TransformCoord(XMVectorSet(focus.x, focus.y, focus.z, 1.0f), view));
	const float texel = kCoverage / static_cast<float>(kResolution);
	const float centerX = floorf(focusInLight.x / texel) * texel;
	const float centerY = floorf(focusInLight.y / texel) * texel;
	const float half = kCoverage * 0.5f;
	const XMMATRIX projection = XMMatrixOrthographicOffCenterLH(centerX - half, centerX + half,
		centerY - half, centerY + half, focusInLight.z - kDepthRange, focusInLight.z + kDepthRange);

	ShadowConstants constants{};
	XMStoreFloat4x4(&constants.LightViewProjection, XMMatrixTranspose(view * projection));
	constants.Parameters = { texel, kDepthBiasMeters / (2.0f * kDepthRange), 1.0f, 1.0f / kResolution };
	context->UpdateSubresource(g_Constants.Get(), 0, nullptr, &constants, 0, 0);
	ID3D11Buffer* buffer = g_Constants.Get();
	context->VSSetConstantBuffers(kConstantSlot, 1, &buffer);
	context->PSSetConstantBuffers(kConstantSlot, 1, &buffer);

	context->OMSetRenderTargets(0, nullptr, g_DepthView.Get());
	context->ClearDepthStencilView(g_DepthView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	D3D11_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(kResolution);
	viewport.Height = static_cast<float>(kResolution);
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);
	context->RSSetState(g_Rasterizer.Get());
	Renderer::SetDepthMode(RenderDepthMode::Enabled);
	s_Rendering = true;
	return true;
}

void ShadowMap::BindCaster(bool alphaTested)
{
	if (!s_Rendering)
	{
		return;
	}
	if (!g_CasterVertexShader) g_CasterVertexShader = ResourceManager::LoadVertexShader("shadowCasterVS.cso");
	if (!g_CasterPixelShader) g_CasterPixelShader = ResourceManager::LoadPixelShader("shadowCasterPS.cso");
	ID3D11DeviceContext* context = Renderer::GetDeviceContext();
	if (!g_CasterVertexShader || context == nullptr)
	{
		return;
	}
	context->IASetInputLayout(g_CasterVertexShader->InputLayout.Get());
	context->VSSetShader(g_CasterVertexShader->Shader.Get(), nullptr, 0);
	// 不透明な物は深度だけ書けばよいのでピクセルシェーダーを外します(描画も速くなります)。
	context->PSSetShader(alphaTested && g_CasterPixelShader ? g_CasterPixelShader->Shader.Get() : nullptr, nullptr, 0);
}

void ShadowMap::End()
{
	if (!s_Rendering)
	{
		return;
	}
	s_Rendering = false;
	ID3D11DeviceContext* context = Renderer::GetDeviceContext();
	if (context == nullptr)
	{
		return;
	}
	// 深度として外してから、受け手用のテクスチャとして繋ぎます。描画先と視点は呼び出し側が元に戻します。
	context->OMSetRenderTargets(0, nullptr, nullptr);
	ID3D11ShaderResourceView* view = g_ShaderView.Get();
	context->PSSetShaderResources(kTextureSlot, 1, &view);
	ID3D11SamplerState* sampler = g_Sampler.Get();
	context->PSSetSamplers(kSamplerSlot, 1, &sampler);
}

void ShadowMap::Disable()
{
	s_Rendering = false;
	ID3D11DeviceContext* context = Renderer::GetDeviceContext();
	if (context == nullptr)
	{
		return;
	}
	ID3D11ShaderResourceView* nullView = nullptr;
	context->PSSetShaderResources(kTextureSlot, 1, &nullView);
	if (g_Constants)
	{
		// 有効フラグ(Parameters.z)を0にすると、受け手の SampleShadow は常に日なた(1)を返します。
		const ShadowConstants constants{};
		context->UpdateSubresource(g_Constants.Get(), 0, nullptr, &constants, 0, 0);
		ID3D11Buffer* buffer = g_Constants.Get();
		context->VSSetConstantBuffers(kConstantSlot, 1, &buffer);
		context->PSSetConstantBuffers(kConstantSlot, 1, &buffer);
	}
}

void ShadowMap::Release()
{
	s_Rendering = false;
	g_CasterVertexShader.reset();
	g_CasterPixelShader.reset();
	g_Constants.Reset();
	g_Rasterizer.Reset();
	g_Sampler.Reset();
	g_ShaderView.Reset();
	g_DepthView.Reset();
	g_Texture.Reset();
}
}
