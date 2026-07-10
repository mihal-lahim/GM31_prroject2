#include "main.h"
#include "renderer.h"
#include "gameObject.h"
#include "modelRenderer.h"
#include "objModelLoader.h"


ComPtr<ID3D11InputLayout> ModelRenderer::m_DefaultVertexLayout = nullptr;
ComPtr<ID3D11VertexShader> ModelRenderer::m_DefaultVertexShader = nullptr;
ComPtr<ID3D11PixelShader> ModelRenderer::m_DefaultPixelShader = nullptr;

namespace
{
	void DebugLog(const std::string& message)
	{
		OutputDebugStringA((message + "\n").c_str());
	}
}

void ModelRenderer::EnsureDefaultShader()
{
	if (m_DefaultVertexLayout == nullptr
		|| m_DefaultVertexShader == nullptr
		|| m_DefaultPixelShader == nullptr)
	{
		Renderer::CreateVertexShader(m_DefaultVertexShader, m_DefaultVertexLayout, "unlitTextureVS.cso");
		Renderer::CreatePixelShader(m_DefaultPixelShader, "unlitTexturePS.cso");
	}
}

void ModelRenderer::Draw()
{
	if (m_Model == nullptr || m_Model->VertexBuffer == nullptr || m_Model->IndexBuffer == nullptr || m_Model->SubsetNum == 0)
	{
		return;
	}

	EnsureDefaultShader();
	if (m_DefaultVertexLayout == nullptr
		|| m_DefaultVertexShader == nullptr
		|| m_DefaultPixelShader == nullptr)
	{
		return;
	}
	Renderer::GetDeviceContext()->IASetInputLayout(m_DefaultVertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_DefaultVertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_DefaultPixelShader.Get(), nullptr, 0);

	if (m_UseOwnerTransform && m_GameObject != nullptr)
	{
		Vector3 scaleValue = m_GameObject->GetScale();
		Vector3 rotationValue = m_GameObject->GetRotation() + m_RotationOffset;
		Vector3 positionValue = m_GameObject->GetPosition();

		XMMATRIX scale = XMMatrixScaling(scaleValue.x, scaleValue.y, scaleValue.z);
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(rotationValue.x, rotationValue.y, rotationValue.z);
		XMMATRIX trans = XMMatrixTranslation(positionValue.x, positionValue.y, positionValue.z);
		Renderer::SetWorldMatrix(scale * rot * trans);
	}

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_Model->VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetIndexBuffer(m_Model->IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (unsigned int i = 0; i < m_Model->SubsetNum; i++)
	{
		Renderer::SetMaterial(m_Model->SubsetArray[i].Material.Material);

		ID3D11ShaderResourceView* texture = m_Model->SubsetArray[i].Material.Texture != nullptr
			? m_Model->SubsetArray[i].Material.Texture->Get()
			: nullptr;
		Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);

		Renderer::GetDeviceContext()->DrawIndexed(m_Model->SubsetArray[i].IndexNum, m_Model->SubsetArray[i].StartIndex, 0);
	}
}

void ModelRenderer::Preload(const char* FileName)
{
	(void)AcquireModel(FileName);
}

void ModelRenderer::ReleaseSharedResources()
{
	m_DefaultVertexLayout.Reset();
	m_DefaultVertexShader.Reset();
	m_DefaultPixelShader.Reset();
}

void ModelRenderer::Load(const char* FileName)
{
	if (FileName == nullptr)
	{
		m_Model.reset();
		return;
	}

	m_Model = AcquireModel(FileName);
}

ModelHandle ModelRenderer::AcquireModel(const char* FileName)
{
	if (FileName == nullptr || FileName[0] == '\0')
	{
		return {};
	}

	const std::string path(FileName);
	return ResourceManager::LoadModel(path, [path]()
	{
		auto model = std::make_shared<MODEL>();
		return ModelRenderer::LoadModel(path.c_str(), model.get())
			? model
			: std::shared_ptr<MODEL>{};
	});
}

ModelRenderer* ModelRenderer::SetUseOwnerTransform(bool enabled)
{
	m_UseOwnerTransform = enabled;
	return this;
}

ModelRenderer* ModelRenderer::SetRotationOffset(const Vector3& offset)
{
	m_RotationOffset = offset;
	return this;
}

bool ModelRenderer::LoadModel(const char* FileName, MODEL* Model)
{
	if (Model == nullptr)
	{
		return false;
	}

	MODEL_OBJ modelObj;
	ObjModelLoader::LoadObj(FileName, &modelObj);
	if (modelObj.VertexArray.empty() || modelObj.IndexArray.empty() || modelObj.SubsetArray.empty())
	{
		DebugLog(std::string("ModelRenderer: no drawable geometry in ") + (FileName != nullptr ? FileName : "(null)"));
		Model->SubsetArray.clear();
		Model->SubsetNum = 0;
		return false;
	}

	D3D11_BUFFER_DESC vertexBufferDesc{};
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(VERTEX_3D) * modelObj.VertexArray.size());
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = modelObj.VertexArray.data();
	HRESULT hr = Renderer::GetDevice()->CreateBuffer(&vertexBufferDesc, &vertexData, Model->VertexBuffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		DebugLog(std::string("ModelRenderer: failed to create vertex buffer for ") + FileName);
		Model->SubsetArray.clear();
		Model->SubsetNum = 0;
		return false;
	}

	D3D11_BUFFER_DESC indexBufferDesc{};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * modelObj.IndexArray.size());
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = modelObj.IndexArray.data();
	hr = Renderer::GetDevice()->CreateBuffer(&indexBufferDesc, &indexData, Model->IndexBuffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		DebugLog(std::string("ModelRenderer: failed to create index buffer for ") + FileName);
		Model->VertexBuffer.Reset();
		Model->SubsetArray.clear();
		Model->SubsetNum = 0;
		return false;
	}

	Model->SubsetArray = modelObj.SubsetArray;
	Model->SubsetNum = static_cast<unsigned int>(Model->SubsetArray.size());

	for (SUBSET& subset : Model->SubsetArray)
	{
		subset.Material.Texture.reset();
		subset.Material.Material.TextureEnable = FALSE;

		if (subset.Material.TextureName.empty())
		{
			continue;
		}

		subset.Material.Texture = ResourceManager::LoadTexture(subset.Material.TextureName);
		if (subset.Material.Texture != nullptr)
		{
			subset.Material.Material.TextureEnable = TRUE;
		}
	}

	return true;
}
