#include "main.h"
#include "renderer.h"
#include "animationModel.h"
#include "DirectXTex.h"
#include "gameObject.h"
#include "graphicsResourceFactory.h"
#include "assimp/config.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	const aiScene* ImportAnimationScene(const char* fileName, unsigned int flags)
	{
		aiPropertyStore* properties = aiCreatePropertyStore();
		if (properties == nullptr)
		{
			return nullptr;
		}

		// FBXのPivotを補助ノードとして残すと、アニメーションキーに含まれる
		// Translation/PreRotationと二重適用になるため、1ボーン1ノードへ統合する。
		aiSetImportPropertyInteger(properties, AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, 0);
		const aiScene* scene = aiImportFileExWithProperties(fileName, flags, nullptr, properties);
		aiReleasePropertyStore(properties);
		return scene;
	}

	template<typename Key>
	unsigned int FindKeyIndex(double animationTime, unsigned int keyCount, const Key* keys)
	{
		if (keyCount < 2 || keys == nullptr)
		{
			return 0;
		}

		for (unsigned int index = 0; index + 1 < keyCount; ++index)
		{
			if (animationTime < keys[index + 1].mTime)
			{
				return index;
			}
		}
		return keyCount - 1;
	}

	aiVector3D SampleVectorKeys(
		double animationTime,
		unsigned int keyCount,
		const aiVectorKey* keys,
		const aiVector3D& fallback)
	{
		if (keyCount == 0 || keys == nullptr)
		{
			return fallback;
		}
		if (keyCount == 1)
		{
			return keys[0].mValue;
		}

		const unsigned int currentIndex = FindKeyIndex(animationTime, keyCount, keys);
		if (currentIndex + 1 >= keyCount)
		{
			return keys[currentIndex].mValue;
		}

		const aiVectorKey& current = keys[currentIndex];
		const aiVectorKey& next = keys[currentIndex + 1];
		const double keyDuration = next.mTime - current.mTime;
		const float factor = keyDuration > 0.0
			? std::clamp(static_cast<float>((animationTime - current.mTime) / keyDuration), 0.0f, 1.0f)
			: 0.0f;
		return current.mValue + (next.mValue - current.mValue) * factor;
	}

	aiQuaternion SampleQuaternionKeys(
		double animationTime,
		unsigned int keyCount,
		const aiQuatKey* keys,
		const aiQuaternion& fallback)
	{
		if (keyCount == 0 || keys == nullptr)
		{
			return fallback;
		}
		if (keyCount == 1)
		{
			return keys[0].mValue;
		}

		const unsigned int currentIndex = FindKeyIndex(animationTime, keyCount, keys);
		if (currentIndex + 1 >= keyCount)
		{
			return keys[currentIndex].mValue;
		}

		const aiQuatKey& current = keys[currentIndex];
		const aiQuatKey& next = keys[currentIndex + 1];
		const double keyDuration = next.mTime - current.mTime;
		const float factor = keyDuration > 0.0
			? std::clamp(static_cast<float>((animationTime - current.mTime) / keyDuration), 0.0f, 1.0f)
			: 0.0f;

		aiQuaternion result;
		aiQuaternion::Interpolate(result, current.mValue, next.mValue, factor);
		return result.Normalize();
	}
}

void AnimationModel::Draw()
{
	ID3D11DeviceContext* context = Renderer::GetDeviceContext();
	if (context == nullptr || m_AiScene == nullptr || m_VertexBuffer == nullptr || m_IndexBuffer == nullptr
		|| m_VertexLayout == nullptr || m_VertexShader == nullptr || m_PixelShader == nullptr)
	{
		return;
	}

	// 他の描画処理が残した状態に依存しないよう、必要なパイプラインを毎回設定する。
	context->IASetInputLayout(m_VertexLayout.Get());
	context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_PixelShader.Get(), nullptr, 0);

	Vector3 position{};
	Vector3 rotation{};
	Vector3 scale{ 1.0f, 1.0f, 1.0f };
	if (m_GameObject != nullptr)
	{
		position = m_GameObject->GetPosition();
		rotation = m_GameObject->GetRotation();
		scale = m_GameObject->GetScale();
	}

	const XMMATRIX world =
		XMMatrixScaling(scale.x * m_ModelScale, scale.y * m_ModelScale, scale.z * m_ModelScale)
		* XMMatrixRotationRollPitchYaw(rotation.x, rotation.y + m_ModelYawOffset, rotation.z)
		* XMMatrixTranslation(position.x, position.y, position.z);
	Renderer::SetWorldMatrix(world);

	// プリミティブトポロジ設定
	context->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// マテリアル設定
	MATERIAL material;
	ZeroMemory(&material, sizeof(material));
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = true;
	Renderer::SetMaterial(material);

	for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
	{
		aiMesh* mesh = m_AiScene->mMeshes[m];


		// マテリアル設定
		aiString texture;
		aiColor3D diffuse(1.0f, 1.0f, 1.0f);
		float opacity = 1.0f;

		aiMaterial* aimaterial = m_AiScene->mMaterials[mesh->mMaterialIndex];
		aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);
		aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
		aimaterial->Get(AI_MATKEY_OPACITY, opacity);

		ID3D11ShaderResourceView* textureView = nullptr;
		if (texture != aiString(""))
		{
			const auto foundTexture = m_Texture.find(texture.C_Str());
			if (foundTexture != m_Texture.end())
			{
				textureView = foundTexture->second;
			}
		}
		context->PSSetShaderResources(0, 1, &textureView);
		material.TextureEnable = textureView != nullptr;

		material.Diffuse = XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, opacity);
		material.Ambient = material.Diffuse;
		Renderer::SetMaterial(material);


		// 頂点バッファ設定
		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, &m_VertexBuffer[m], &stride, &offset);

		// インデックスバッファ設定
		context->IASetIndexBuffer(m_IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

		// ポリゴン描画
		context->DrawIndexed(mesh->mNumFaces * 3, 0, 0);
	}
}

void AnimationModel::Load(const char* FileName)
{
	const std::string modelPath(FileName);
	if (!Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "AnimationModel"))
	{
		OutputDebugStringA("AnimationModel: failed to create shaders.\n");
		return;
	}

	m_AiScene = ImportAnimationScene(
		FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);
	assert(m_AiScene);

	m_VertexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];
	m_IndexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];


	//変形後頂点配列生成
	m_DeformVertex = new std::vector<DEFORM_VERTEX>[m_AiScene->mNumMeshes];

	//再帰的にボーン生成
	CreateBone(m_AiScene->mRootNode);



	for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
	{
		aiMesh* mesh = m_AiScene->mMeshes[m];

		// 頂点バッファ生成
		{
			VERTEX_3D* vertex = new VERTEX_3D[mesh->mNumVertices];

			for (unsigned int v = 0; v < mesh->mNumVertices; v++)
			{
				vertex[v].Position = XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
				vertex[v].Normal = XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
				vertex[v].TexCoord = XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
				vertex[v].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			}

			D3D11_BUFFER_DESC bd;
			ZeroMemory(&bd, sizeof(bd));
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.ByteWidth = sizeof(VERTEX_3D) * mesh->mNumVertices;
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			D3D11_SUBRESOURCE_DATA sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.pSysMem = vertex;

			Renderer::GetDevice()->CreateBuffer(&bd, &sd,
				&m_VertexBuffer[m]);

			delete[] vertex;
		}


		// インデックスバッファ生成
		{
			unsigned int* index = new unsigned int[mesh->mNumFaces * 3];

			for (unsigned int f = 0; f < mesh->mNumFaces; f++)
			{
				const aiFace* face = &mesh->mFaces[f];

				assert(face->mNumIndices == 3);

				index[f * 3 + 0] = face->mIndices[0];
				index[f * 3 + 1] = face->mIndices[1];
				index[f * 3 + 2] = face->mIndices[2];
			}

			D3D11_BUFFER_DESC bd;
			ZeroMemory(&bd, sizeof(bd));
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = sizeof(unsigned int) * mesh->mNumFaces * 3;
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bd.CPUAccessFlags = 0;

			D3D11_SUBRESOURCE_DATA sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.pSysMem = index;

			Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_IndexBuffer[m]);

			delete[] index;
		}



		//変形後頂点データ初期化
		for (unsigned int v = 0; v < mesh->mNumVertices; v++)
		{
			DEFORM_VERTEX deformVertex;
			deformVertex.Position = mesh->mVertices[v];
			deformVertex.Normal = mesh->mNormals[v];
			deformVertex.BoneNum = 0;

			for (unsigned int b = 0; b < 4; b++)
			{
				deformVertex.BoneName[b] = "";
				deformVertex.BoneWeight[b] = 0.0f;
				deformVertex.BoneOffsetMatrix[b] = aiMatrix4x4();
			}

			m_DeformVertex[m].push_back(deformVertex);
		}


		//ボーンデータ初期化
		for (unsigned int b = 0; b < mesh->mNumBones; b++)
		{
			aiBone* bone = mesh->mBones[b];

			//変形後頂点にボーンデータ格納
			for (unsigned int w = 0; w < bone->mNumWeights; w++)
			{
				aiVertexWeight weight = bone->mWeights[w];

				int num = m_DeformVertex[m][weight.mVertexId].BoneNum;

				m_DeformVertex[m][weight.mVertexId].BoneWeight[num] = weight.mWeight;
				m_DeformVertex[m][weight.mVertexId].BoneName[num] = bone->mName.C_Str();
				m_DeformVertex[m][weight.mVertexId].BoneOffsetMatrix[num] = bone->mOffsetMatrix;
				m_DeformVertex[m][weight.mVertexId].BoneNum++;

				assert(m_DeformVertex[m][weight.mVertexId].BoneNum <= 4);
			}
		}
	}



	//テクスチャ読み込み
	for (unsigned int i = 0; i < m_AiScene->mNumTextures; i++)
	{
		aiTexture* aitexture = m_AiScene->mTextures[i];

		ID3D11ShaderResourceView* texture = nullptr;

		// テクスチャ読み込み
		TexMetadata metadata{};
		ScratchImage image{};
		const HRESULT loadResult = LoadFromWICMemory(
			aitexture->pcData, aitexture->mWidth, WIC_FLAGS_NONE, &metadata, image);
		if (SUCCEEDED(loadResult))
		{
			CreateShaderResourceView(
				Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
		}

		if (texture != nullptr)
		{
			m_Texture[aitexture->mFilename.data] = texture;
		}
	}



}



void AnimationModel::LoadAnimation(const char* FileName, const char* Name)
{

	m_Animation[Name] = ImportAnimationScene(FileName, aiProcess_ConvertToLeftHanded);
	assert(m_Animation[Name]);

}


void AnimationModel::CreateBone(aiNode* node)
{
	BONE bone{};
	bone.AnimationMatrix = node->mTransformation;

	m_Bone[node->mName.C_Str()] = bone;

	for (unsigned int n = 0; n < node->mNumChildren; n++)
	{
		CreateBone(node->mChildren[n]);
	}

}


void AnimationModel::Uninit()
{
	if (m_AiScene != nullptr)
	{
		for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
		{
			if (m_VertexBuffer != nullptr && m_VertexBuffer[m] != nullptr)
			{
				m_VertexBuffer[m]->Release();
			}
			if (m_IndexBuffer != nullptr && m_IndexBuffer[m] != nullptr)
			{
				m_IndexBuffer[m]->Release();
			}
		}
	}

	delete[] m_VertexBuffer;
	delete[] m_IndexBuffer;

	delete[] m_DeformVertex;


	for (std::pair<const std::string, ID3D11ShaderResourceView*> pair : m_Texture)
	{
		if (pair.second != nullptr)
		{
			pair.second->Release();
		}
	}
	m_Texture.clear();



	if (m_AiScene != nullptr)
	{
		aiReleaseImport(m_AiScene);
		m_AiScene = nullptr;
	}


	for (std::pair<const std::string, const aiScene*> pair : m_Animation)
	{
		aiReleaseImport(pair.second);
	}
	m_Animation.clear();
	m_VertexLayout.Reset();
	m_VertexShader.Reset();
	m_PixelShader.Reset();

}





void AnimationModel::Update(const char* AnimationName1, int Frame1)
{
	if (AnimationName1 == nullptr || m_AiScene == nullptr || m_AiScene->mRootNode == nullptr
		|| m_DeformVertex == nullptr || m_VertexBuffer == nullptr)
	{
		return;
	}

	const auto foundScene = m_Animation.find(AnimationName1);
	if (foundScene == m_Animation.end() || foundScene->second == nullptr
		|| foundScene->second->mNumAnimations == 0 || foundScene->second->mAnimations[0] == nullptr)
	{
		return;
	}

	const aiAnimation* animation = foundScene->second->mAnimations[0];
	const double duration = animation->mDuration > 0.0 ? animation->mDuration : 1.0;
	const double animationTime = std::fmod(static_cast<double>((std::max)(Frame1, 0)), duration);

	// 前のアニメーションで上書きしたローカル行列を、モデル本来の姿勢へ戻す。
	auto resetLocalMatrices = [this](auto&& self, aiNode* node) -> void
	{
		const auto foundBone = m_Bone.find(node->mName.C_Str());
		if (foundBone != m_Bone.end())
		{
			foundBone->second.AnimationMatrix = node->mTransformation;
		}
		for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
		{
			self(self, node->mChildren[childIndex]);
		}
	};
	resetLocalMatrices(resetLocalMatrices, m_AiScene->mRootNode);

	// チャンネルごとに位置・回転・拡縮を補間し、対応するボーンのローカル行列を作る。
	for (unsigned int channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
	{
		const aiNodeAnim* channel = animation->mChannels[channelIndex];
		if (channel == nullptr)
		{
			continue;
		}

		const auto foundBone = m_Bone.find(channel->mNodeName.C_Str());
		if (foundBone == m_Bone.end())
		{
			continue;
		}

		aiVector3D bindScale;
		aiQuaternion bindRotation;
		aiVector3D bindPosition;
		foundBone->second.AnimationMatrix.Decompose(bindScale, bindRotation, bindPosition);

		const aiVector3D position = SampleVectorKeys(
			animationTime, channel->mNumPositionKeys, channel->mPositionKeys, bindPosition);
		const aiQuaternion rotation = SampleQuaternionKeys(
			animationTime, channel->mNumRotationKeys, channel->mRotationKeys, bindRotation);
		const aiVector3D scale = SampleVectorKeys(
			animationTime, channel->mNumScalingKeys, channel->mScalingKeys, bindScale);

		foundBone->second.AnimationMatrix = aiMatrix4x4(scale, rotation, position);
	}

	UpdateBoneMatrix(m_AiScene->mRootNode, aiMatrix4x4());

	aiMatrix4x4 globalInverse = m_AiScene->mRootNode->mTransformation;
	globalInverse.Inverse();
	ID3D11DeviceContext* context = Renderer::GetDeviceContext();
	if (context == nullptr)
	{
		return;
	}

	// 各頂点へ最大4本のボーン変換を重み付きで適用し、動的頂点バッファを更新する。
	for (unsigned int meshIndex = 0; meshIndex < m_AiScene->mNumMeshes; ++meshIndex)
	{
		const aiMesh* mesh = m_AiScene->mMeshes[meshIndex];
		if (mesh == nullptr || m_VertexBuffer[meshIndex] == nullptr)
		{
			continue;
		}

		D3D11_MAPPED_SUBRESOURCE mappedResource{};
		if (!Engine::GraphicsResourceFactory::MapWriteDiscard(
			m_VertexBuffer[meshIndex], mappedResource, "AnimationModel"))
		{
			continue;
		}

		VERTEX_3D* vertices = static_cast<VERTEX_3D*>(mappedResource.pData);
		for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
		{
			const DEFORM_VERTEX& source = m_DeformVertex[meshIndex][vertexIndex];
			aiVector3D position{};
			aiVector3D normal{};
			float totalWeight = 0.0f;

			const int influenceCount = (std::min)(source.BoneNum, 4);
			for (int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
			{
				const float weight = source.BoneWeight[influenceIndex];
				const auto foundBone = m_Bone.find(source.BoneName[influenceIndex]);
				if (weight <= 0.0f || foundBone == m_Bone.end())
				{
					continue;
				}

				const aiMatrix4x4 transform = globalInverse
					* foundBone->second.Matrix
					* source.BoneOffsetMatrix[influenceIndex];
				position += (transform * source.Position) * weight;
				normal += (aiMatrix3x3(transform) * source.Normal) * weight;
				totalWeight += weight;
			}

			if (totalWeight > 0.0f)
			{
				position /= totalWeight;
				normal /= totalWeight;
				normal.Normalize();
			}
			else
			{
				position = source.Position;
				normal = source.Normal;
			}

			vertices[vertexIndex].Position = XMFLOAT3(position.x, position.y, position.z);
			vertices[vertexIndex].Normal = XMFLOAT3(normal.x, normal.y, normal.z);
			vertices[vertexIndex].TexCoord = XMFLOAT2(
				mesh->mTextureCoords[0][vertexIndex].x,
				mesh->mTextureCoords[0][vertexIndex].y);
			vertices[vertexIndex].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		}

		context->Unmap(m_VertexBuffer[meshIndex], 0);
	}

}


void AnimationModel::UpdateBoneMatrix(aiNode* node, aiMatrix4x4 matrix)
{
	if (node == nullptr)
	{
		return;
	}

	const auto foundBone = m_Bone.find(node->mName.C_Str());
	const aiMatrix4x4 localMatrix = foundBone != m_Bone.end()
		? foundBone->second.AnimationMatrix
		: node->mTransformation;
	const aiMatrix4x4 globalMatrix = matrix * localMatrix;
	if (foundBone != m_Bone.end())
	{
		foundBone->second.Matrix = globalMatrix;
	}

	for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
	{
		UpdateBoneMatrix(node->mChildren[childIndex], globalMatrix);
	}

}

