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
	ID3D11Device* device = Renderer::GetDevice();
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
	if (Renderer::CreateVertexShader(vertexShader, inputLayout, "unlitTextureVS.cso")
		&& Renderer::CreatePixelShader(pixelShader, "unlitTexturePS.cso"))
	{
		return true;
	}

	vertexShader.Reset();
	inputLayout.Reset();
	pixelShader.Reset();
	LogGpuCreationFailure(ownerName, "unlit shader pair", E_FAIL);
	return false;
}
