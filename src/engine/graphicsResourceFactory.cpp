#include "graphicsResourceFactory.h"

#include <cstdio>

namespace
{
	void LogGpuCreationFailure(const char* ownerName, const char* resourceName, HRESULT result)
	{
		char message[256]{};
		sprintf_s(
			message,
			"GPU resource creation failed: owner=%s, resource=%s, HRESULT=0x%08X.\n",
			ownerName != nullptr ? ownerName : "unknown",
			resourceName,
			static_cast<unsigned int>(result));
		OutputDebugStringA(message);
	}
}

bool Engine::GraphicsResourceFactory::CreateBuffer(
	ComPtr<ID3D11Buffer>& buffer,
	const D3D11_BUFFER_DESC& description,
	const D3D11_SUBRESOURCE_DATA* initialData,
	const char* ownerName)
{
	buffer.Reset();
	ID3D11Device* device = Engine::Renderer::GetDevice();
	if (device == nullptr)
	{
		LogGpuCreationFailure(ownerName, "vertex/index/constant buffer", E_POINTER);
		return false;
	}

	const HRESULT result = device->CreateBuffer(&description, initialData, buffer.GetAddressOf());
	if (FAILED(result))
	{
		buffer.Reset();
		LogGpuCreationFailure(ownerName, "vertex/index/constant buffer", result);
		return false;
	}
	return true;
}

bool Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
	ComPtr<ID3D11VertexShader>& vertexShader,
	ComPtr<ID3D11InputLayout>& inputLayout,
	ComPtr<ID3D11PixelShader>& pixelShader,
	const char* ownerName)
{
	if (Engine::Renderer::CreateVertexShader(vertexShader, inputLayout, "unlitTextureVS.cso")
		&& Engine::Renderer::CreatePixelShader(pixelShader, "unlitTexturePS.cso"))
	{
		return true;
	}

	vertexShader.Reset();
	inputLayout.Reset();
	pixelShader.Reset();
	LogGpuCreationFailure(ownerName, "unlit shader pair", E_FAIL);
	return false;
}

bool Engine::GraphicsResourceFactory::IsDrawable(
	ID3D11Buffer* vertexBuffer,
	ID3D11InputLayout* inputLayout,
	ID3D11VertexShader* vertexShader,
	ID3D11PixelShader* pixelShader) noexcept
{
	return Engine::Renderer::GetDeviceContext() != nullptr
		&& vertexBuffer != nullptr
		&& inputLayout != nullptr
		&& vertexShader != nullptr
		&& pixelShader != nullptr;
}

bool Engine::GraphicsResourceFactory::CreateTextureFromPixels(
	ComPtr<ID3D11ShaderResourceView>& view,
	UINT width,
	UINT height,
	const std::uint32_t* pixels,
	const char* ownerName)
{
	view.Reset();
	ID3D11Device* device = Engine::Renderer::GetDevice();
	if (device == nullptr || pixels == nullptr || width == 0 || height == 0)
	{
		LogGpuCreationFailure(ownerName, "pixel texture", E_POINTER);
		return false;
	}

	D3D11_TEXTURE2D_DESC description{};
	description.Width = width;
	description.Height = height;
	description.MipLevels = 1;
	description.ArraySize = 1;
	description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	description.SampleDesc.Count = 1;
	description.Usage = D3D11_USAGE_IMMUTABLE;
	description.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = pixels;
	data.SysMemPitch = width * sizeof(std::uint32_t);

	ComPtr<ID3D11Texture2D> texture;
	HRESULT result = device->CreateTexture2D(&description, &data, texture.GetAddressOf());
	if (FAILED(result))
	{
		LogGpuCreationFailure(ownerName, "pixel texture", result);
		return false;
	}
	result = device->CreateShaderResourceView(texture.Get(), nullptr, view.GetAddressOf());
	if (FAILED(result))
	{
		view.Reset();
		LogGpuCreationFailure(ownerName, "pixel texture view", result);
		return false;
	}
	return true;
}

bool Engine::GraphicsResourceFactory::MapWriteDiscard(
	ID3D11Buffer* buffer,
	D3D11_MAPPED_SUBRESOURCE& mappedResource,
	const char* ownerName)
{
	mappedResource = {};
	ID3D11DeviceContext* context = Engine::Renderer::GetDeviceContext();
	if (context == nullptr || buffer == nullptr)
	{
		LogGpuCreationFailure(ownerName, "dynamic buffer map", E_POINTER);
		return false;
	}

	const HRESULT result = context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		LogGpuCreationFailure(ownerName, "dynamic buffer map", result);
		mappedResource = {};
		return false;
	}
	return true;
}
