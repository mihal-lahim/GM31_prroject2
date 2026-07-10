#include "main.h"
#include "renderer.h"
#include "gameObject.h"
#include "modelRenderer.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

ComPtr<ID3D11InputLayout> ModelRenderer::m_DefaultVertexLayout = nullptr;
ComPtr<ID3D11VertexShader> ModelRenderer::m_DefaultVertexShader = nullptr;
ComPtr<ID3D11PixelShader> ModelRenderer::m_DefaultPixelShader = nullptr;

namespace
{
	void DebugLog(const std::string& message)
	{
		OutputDebugStringA((message + "\n").c_str());
	}

	MATERIAL MakeDefaultMaterialData()
	{
		MATERIAL material{};
		material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material.Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		material.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		material.Shininess = 0.0f;
		material.TextureEnable = FALSE;
		return material;
	}

	MODEL_MATERIAL MakeDefaultMaterial(const std::string& name = "default")
	{
		MODEL_MATERIAL material{};
		material.Name = name;
		material.Material = MakeDefaultMaterialData();
		return material;
	}

	std::string Trim(const std::string& value)
	{
		const char* whitespace = " \t\r\n";
		size_t first = value.find_first_not_of(whitespace);
		if (first == std::string::npos)
		{
			return "";
		}

		size_t last = value.find_last_not_of(whitespace);
		return value.substr(first, last - first + 1);
	}

	std::string RemoveComment(const std::string& line)
	{
		size_t comment = line.find('#');
		if (comment == std::string::npos)
		{
			return line;
		}

		return line.substr(0, comment);
	}

	bool TryParseInt(const std::string& value, int& result)
	{
		if (value.empty())
		{
			return false;
		}

		char* end = nullptr;
		long parsed = strtol(value.c_str(), &end, 10);
		if (end == value.c_str() || *end != '\0')
		{
			return false;
		}

		result = static_cast<int>(parsed);
		return true;
	}

	std::vector<std::string> Split(const std::string& value, char delimiter)
	{
		std::vector<std::string> parts;
		std::string part;
		std::stringstream stream(value);
		while (std::getline(stream, part, delimiter))
		{
			parts.push_back(part);
		}

		return parts;
	}

	int ResolveObjIndex(int objIndex, size_t elementCount)
	{
		if (objIndex > 0)
		{
			int resolved = objIndex - 1;
			if (resolved >= 0 && static_cast<size_t>(resolved) < elementCount)
			{
				return resolved;
			}
		}
		else if (objIndex < 0)
		{
			int resolved = static_cast<int>(elementCount) + objIndex;
			if (resolved >= 0 && static_cast<size_t>(resolved) < elementCount)
			{
				return resolved;
			}
		}

		return -1;
	}

	struct ObjVertexRef
	{
		int Position = -1;
		int TexCoord = -1;
		int Normal = -1;
	};

	bool ParseObjVertexRef(const std::string& token, const std::vector<XMFLOAT3>& positions, const std::vector<XMFLOAT2>& texcoords, const std::vector<XMFLOAT3>& normals, ObjVertexRef& ref)
	{
		std::vector<std::string> parts = Split(token, '/');
		if (parts.empty())
		{
			return false;
		}

		int index = 0;
		if (!TryParseInt(parts[0], index))
		{
			return false;
		}
		ref.Position = ResolveObjIndex(index, positions.size());
		if (ref.Position < 0)
		{
			return false;
		}

		if (parts.size() > 1 && TryParseInt(parts[1], index))
		{
			ref.TexCoord = ResolveObjIndex(index, texcoords.size());
		}
		if (parts.size() > 2 && TryParseInt(parts[2], index))
		{
			ref.Normal = ResolveObjIndex(index, normals.size());
		}

		return true;
	}

	void EnsureSubset(MODEL_OBJ& modelObj, const MODEL_MATERIAL& material)
	{
		if (!modelObj.SubsetArray.empty())
		{
			return;
		}

		SUBSET subset{};
		subset.StartIndex = static_cast<unsigned int>(modelObj.IndexArray.size());
		subset.IndexNum = 0;
		subset.Material = material;
		modelObj.SubsetArray.push_back(subset);
	}

	void BeginSubset(MODEL_OBJ& modelObj, const MODEL_MATERIAL& material)
	{
		if (!modelObj.SubsetArray.empty() && modelObj.SubsetArray.back().IndexNum == 0)
		{
			modelObj.SubsetArray.back().Material = material;
			return;
		}

		SUBSET subset{};
		subset.StartIndex = static_cast<unsigned int>(modelObj.IndexArray.size());
		subset.IndexNum = 0;
		subset.Material = material;
		modelObj.SubsetArray.push_back(subset);
	}

	void UpdateCurrentSubsetIndexCount(MODEL_OBJ& modelObj)
	{
		if (modelObj.SubsetArray.empty())
		{
			return;
		}

		SUBSET& subset = modelObj.SubsetArray.back();
		subset.IndexNum = static_cast<unsigned int>(modelObj.IndexArray.size()) - subset.StartIndex;
	}

	bool AppendObjVertex(const ObjVertexRef& ref, const std::vector<XMFLOAT3>& positions, const std::vector<XMFLOAT2>& texcoords, const std::vector<XMFLOAT3>& normals, MODEL_OBJ& modelObj, unsigned int& vertexIndex)
	{
		if (ref.Position < 0 || static_cast<size_t>(ref.Position) >= positions.size())
		{
			return false;
		}

		VERTEX_3D vertex{};
		vertex.Position = positions[ref.Position];
		vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex.TexCoord = XMFLOAT2(0.0f, 0.0f);

		if (ref.TexCoord >= 0 && static_cast<size_t>(ref.TexCoord) < texcoords.size())
		{
			vertex.TexCoord = texcoords[ref.TexCoord];
		}
		if (ref.Normal >= 0 && static_cast<size_t>(ref.Normal) < normals.size())
		{
			vertex.Normal = normals[ref.Normal];
		}

		vertexIndex = static_cast<unsigned int>(modelObj.VertexArray.size());
		modelObj.VertexArray.push_back(vertex);
		return true;
	}

	std::filesystem::path ResolveSiblingPath(const std::filesystem::path& baseFile, const std::string& relativePath)
	{
		std::filesystem::path path(relativePath);
		if (path.is_absolute())
		{
			return path.lexically_normal();
		}

		return (baseFile.parent_path() / path).lexically_normal();
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
	LoadObj(FileName, &modelObj);
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

void ModelRenderer::LoadObj(const char* FileName, MODEL_OBJ* ModelObj)
{
	if (ModelObj == nullptr)
	{
		return;
	}

	*ModelObj = MODEL_OBJ{};
	if (FileName == nullptr)
	{
		DebugLog("ModelRenderer: OBJ file name is null.");
		return;
	}

	std::filesystem::path objPath(FileName);
	std::ifstream file(objPath);
	if (!file)
	{
		DebugLog(std::string("ModelRenderer: failed to open OBJ ") + FileName);
		return;
	}

	std::vector<XMFLOAT3> positions;
	std::vector<XMFLOAT3> normals;
	std::vector<XMFLOAT2> texcoords;
	std::unordered_map<std::string, MODEL_MATERIAL> materials;
	MODEL_MATERIAL currentMaterial = MakeDefaultMaterial();

	std::string line;
	while (std::getline(file, line))
	{
		line = Trim(RemoveComment(line));
		if (line.empty())
		{
			continue;
		}

		std::stringstream stream(line);
		std::string token;
		stream >> token;

		if (token == "mtllib")
		{
			std::string materialFileName;
			stream >> materialFileName;
			if (!materialFileName.empty())
			{
				std::filesystem::path materialPath = ResolveSiblingPath(objPath, materialFileName);
				LoadMaterial(materialPath.string().c_str(), materials);
			}
		}
		else if (token == "usemtl")
		{
			std::string materialName;
			stream >> materialName;
			auto material = materials.find(materialName);
			currentMaterial = (material != materials.end()) ? material->second : MakeDefaultMaterial(materialName);
			BeginSubset(*ModelObj, currentMaterial);
		}
		else if (token == "v")
		{
			XMFLOAT3 position{};
			if (stream >> position.x >> position.y >> position.z)
			{
				positions.push_back(position);
			}
		}
		else if (token == "vn")
		{
			XMFLOAT3 normal{};
			if (stream >> normal.x >> normal.y >> normal.z)
			{
				normals.push_back(normal);
			}
		}
		else if (token == "vt")
		{
			XMFLOAT2 texcoord{};
			if (stream >> texcoord.x >> texcoord.y)
			{
				texcoord.x = 1.0f - texcoord.x;
				texcoord.y = 1.0f - texcoord.y;
				texcoords.push_back(texcoord);
			}
		}
		else if (token == "f")
		{
			EnsureSubset(*ModelObj, currentMaterial);

			std::vector<unsigned int> faceIndices;
			std::string faceToken;
			while (stream >> faceToken)
			{
				ObjVertexRef ref;
				if (!ParseObjVertexRef(faceToken, positions, texcoords, normals, ref))
				{
					continue;
				}

				unsigned int vertexIndex = 0;
				if (AppendObjVertex(ref, positions, texcoords, normals, *ModelObj, vertexIndex))
				{
					faceIndices.push_back(vertexIndex);
				}
			}

			if (faceIndices.size() >= 3)
			{
				for (size_t i = 1; i + 1 < faceIndices.size(); i++)
				{
					ModelObj->IndexArray.push_back(faceIndices[0]);
					ModelObj->IndexArray.push_back(faceIndices[i]);
					ModelObj->IndexArray.push_back(faceIndices[i + 1]);
				}
				UpdateCurrentSubsetIndexCount(*ModelObj);
			}
		}
	}

	ModelObj->SubsetArray.erase(
		std::remove_if(ModelObj->SubsetArray.begin(), ModelObj->SubsetArray.end(), [](const SUBSET& subset)
			{
				return subset.IndexNum == 0;
			}),
		ModelObj->SubsetArray.end());

	ModelObj->VertexNum = static_cast<unsigned int>(ModelObj->VertexArray.size());
	ModelObj->IndexNum = static_cast<unsigned int>(ModelObj->IndexArray.size());
	ModelObj->SubsetNum = static_cast<unsigned int>(ModelObj->SubsetArray.size());
}

void ModelRenderer::LoadMaterial(const char* FileName, std::unordered_map<std::string, MODEL_MATERIAL>& MaterialMap)
{
	if (FileName == nullptr)
	{
		return;
	}

	std::filesystem::path materialPath(FileName);
	std::ifstream file(materialPath);
	if (!file)
	{
		DebugLog(std::string("ModelRenderer: failed to open MTL ") + FileName);
		return;
	}

	MODEL_MATERIAL* currentMaterial = nullptr;
	std::string line;
	while (std::getline(file, line))
	{
		line = Trim(RemoveComment(line));
		if (line.empty())
		{
			continue;
		}

		std::stringstream stream(line);
		std::string token;
		stream >> token;

		if (token == "newmtl")
		{
			std::string name;
			stream >> name;
			if (!name.empty())
			{
				MaterialMap[name] = MakeDefaultMaterial(name);
				currentMaterial = &MaterialMap[name];
			}
		}
		else if (currentMaterial == nullptr)
		{
			continue;
		}
		else if (token == "Ka")
		{
			stream >> currentMaterial->Material.Ambient.x >> currentMaterial->Material.Ambient.y >> currentMaterial->Material.Ambient.z;
			currentMaterial->Material.Ambient.w = 1.0f;
		}
		else if (token == "Kd")
		{
			stream >> currentMaterial->Material.Diffuse.x >> currentMaterial->Material.Diffuse.y >> currentMaterial->Material.Diffuse.z;
			currentMaterial->Material.Diffuse.w = 1.0f;
		}
		else if (token == "Ks")
		{
			stream >> currentMaterial->Material.Specular.x >> currentMaterial->Material.Specular.y >> currentMaterial->Material.Specular.z;
			currentMaterial->Material.Specular.w = 1.0f;
		}
		else if (token == "Ns")
		{
			stream >> currentMaterial->Material.Shininess;
		}
		else if (token == "d")
		{
			stream >> currentMaterial->Material.Diffuse.w;
		}
		else if (token == "map_Kd")
		{
			std::string textureFileName;
			stream >> textureFileName;
			if (!textureFileName.empty())
			{
				currentMaterial->TextureName = ResolveSiblingPath(materialPath, textureFileName).string();
			}
		}
	}
}
