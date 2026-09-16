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
	if (!m_Visible)
	{
		return;
	}

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

	Renderer::SetWorldMatrix(GetWorldMatrix());

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
	m_SkinTransforms.resize(m_AiScene->mNumMeshes);

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
				deformVertex.BoneIndex[b] = -1;
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
				m_DeformVertex[m][weight.mVertexId].BoneIndex[num] = static_cast<int>(b);
				m_DeformVertex[m][weight.mVertexId].BoneName[num] = bone->mName.C_Str();
				m_DeformVertex[m][weight.mVertexId].BoneOffsetMatrix[num] = bone->mOffsetMatrix;
				m_DeformVertex[m][weight.mVertexId].BoneNum++;

				assert(m_DeformVertex[m][weight.mVertexId].BoneNum <= 4);
			}
		}
		for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
		{
			const DEFORM_VERTEX& source = m_DeformVertex[m][v];
			for (int influence = 0; influence < (std::min)(source.BoneNum, 4); ++influence)
			{
				if (source.BoneWeight[influence] < 0.25f) continue;
				const std::string& name = source.BoneName[influence];
				const int foot = name.find("LeftFoot") != std::string::npos
					|| name.find("LeftToe") != std::string::npos ? 0
					: name.find("RightFoot") != std::string::npos
					|| name.find("RightToe") != std::string::npos ? 1 : -1;
				if (foot >= 0)
				{
					m_FootVertexRefs[foot].emplace_back(m, v);
					break;
				}
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
	m_CurrentAnimation.clear();
	m_BlendSourcePose.clear();
	m_AnimationSeconds = 0.0f;
	m_BlendSeconds = 0.0f;
	m_BlendDuration = 0.0f;
	m_GroundHeight = nullptr;
	m_FootLocalVertices[0].clear();
	m_FootLocalVertices[1].clear();
	m_FootVertexRefs[0].clear();
	m_FootVertexRefs[1].clear();
	m_SkinTransforms.clear();
	m_VertexLayout.Reset();
	m_VertexShader.Reset();
	m_PixelShader.Reset();

}





void AnimationModel::PlayAnimation(const char* name, float blendSeconds)
{
	if (name == nullptr || m_CurrentAnimation == name)
	{
		return;
	}
	const auto foundScene = m_Animation.find(name);
	if (foundScene == m_Animation.end() || foundScene->second == nullptr
		|| foundScene->second->mNumAnimations == 0 || foundScene->second->mAnimations[0] == nullptr)
	{
		return;
	}
	const bool hasCurrentPose = !m_CurrentAnimation.empty();
	m_BlendSourcePose.clear();
	if (hasCurrentPose && blendSeconds > 0.0f)
	{
		for (const auto& [boneName, bone] : m_Bone)
		{
			m_BlendSourcePose.emplace(boneName, bone.AnimationMatrix);
		}
	}
	m_CurrentAnimation = name;
	m_AnimationSeconds = 0.0f;
	m_BlendSeconds = 0.0f;
	m_BlendDuration = hasCurrentPose ? (std::max)(blendSeconds, 0.0f) : 0.0f;
}

void AnimationModel::TickAnimation(float deltaSeconds, bool uploadMesh)
{
	if (m_CurrentAnimation.empty()) return;
	const auto foundScene = m_Animation.find(m_CurrentAnimation);
	if (foundScene == m_Animation.end() || foundScene->second == nullptr
		|| foundScene->second->mNumAnimations == 0 || foundScene->second->mAnimations[0] == nullptr)
	{
		return;
	}
	const aiAnimation* animation = foundScene->second->mAnimations[0];
	const float step = (std::max)(deltaSeconds, 0.0f);
	m_AnimationSeconds += step;
	m_BlendSeconds += step;
	const double ticksPerSecond = animation->mTicksPerSecond > 0.0
		? animation->mTicksPerSecond : 25.0;
	const double duration = animation->mDuration > 0.0 ? animation->mDuration : 1.0;
	const double animationTime = std::fmod(m_AnimationSeconds * ticksPerSecond, duration);
	const float blendWeight = m_BlendDuration > 0.0f
		? std::clamp(m_BlendSeconds / m_BlendDuration, 0.0f, 1.0f) : 1.0f;
	ApplyAnimation(animation, animationTime, blendWeight, uploadMesh);
	if (blendWeight >= 1.0f)
	{
		m_BlendSourcePose.clear();
		m_BlendDuration = 0.0f;
	}
}

void AnimationModel::Update(const char* animationName, int frame)
{
	if (animationName == nullptr) return;
	const auto foundScene = m_Animation.find(animationName);
	if (foundScene == m_Animation.end() || foundScene->second == nullptr
		|| foundScene->second->mNumAnimations == 0 || foundScene->second->mAnimations[0] == nullptr)
	{
		return;
	}

	const aiAnimation* animation = foundScene->second->mAnimations[0];
	const double duration = animation->mDuration > 0.0 ? animation->mDuration : 1.0;
	const double animationTime = std::fmod(static_cast<double>((std::max)(frame, 0)), duration);
	ApplyAnimation(animation, animationTime, 1.0f, true);
}

bool AnimationModel::GetRequiredGroundOffset(float (*heightAt)(float, float), float& offset) const
{
	if (heightAt == nullptr || m_GameObject == nullptr)
	{
		return false;
	}
	const XMMATRIX world = GetWorldMatrix();
	bool found = false;
	offset = -1000.0f;
	for (const auto& foot : m_FootLocalVertices)
	{
		for (const Vector3& local : foot)
		{
			XMFLOAT3 point{};
			XMStoreFloat3(&point, XMVector3TransformCoord(
				XMVectorSet(local.x, local.y, local.z, 1.0f), world));
			offset = (std::max)(offset, heightAt(point.x, point.z) - point.y);
			found = true;
		}
	}
	return found;
}

bool AnimationModel::GetFootClearance(float (*heightAt)(float, float), float (&clearance)[2]) const
{
	if (heightAt == nullptr || m_GameObject == nullptr
		|| m_FootLocalVertices[0].empty() || m_FootLocalVertices[1].empty()) return false;
	const XMMATRIX world = GetWorldMatrix();
	for (int footIndex = 0; footIndex < 2; ++footIndex)
	{
		clearance[footIndex] = 1000.0f;
		for (const Vector3& local : m_FootLocalVertices[footIndex])
		{
			XMFLOAT3 point{};
			XMStoreFloat3(&point, XMVector3TransformCoord(
				XMVectorSet(local.x, local.y, local.z, 1.0f), world));
			clearance[footIndex] = (std::min)(clearance[footIndex],
				point.y - heightAt(point.x, point.z));
		}
	}
	return true;
}

XMMATRIX AnimationModel::GetWorldMatrix() const
{
	if (m_GameObject == nullptr) return XMMatrixIdentity();
	const Vector3 position = m_GameObject->GetPosition();
	const Vector3 rotation = m_GameObject->GetRotation();
	const Vector3 scale = m_GameObject->GetScale();
	return XMMatrixScaling(scale.x * m_ModelScale, scale.y * m_ModelScale, scale.z * m_ModelScale)
		* XMMatrixRotationRollPitchYaw(rotation.x, rotation.y + m_ModelYawOffset, rotation.z)
		* XMMatrixTranslation(position.x, position.y, position.z);
}

void AnimationModel::SolveLegIK(
	const char* upperName, const char* lowerName, const char* footName, int footIndex)
{
	if (m_GroundHeight == nullptr || m_AiScene == nullptr || footIndex < 0 || footIndex > 1)
		return;
	aiNode* upperNode = m_AiScene->mRootNode->FindNode(upperName);
	aiNode* lowerNode = m_AiScene->mRootNode->FindNode(lowerName);
	aiNode* footNode = m_AiScene->mRootNode->FindNode(footName);
	if (upperNode == nullptr || lowerNode == nullptr || footNode == nullptr
		|| upperNode->mParent == nullptr || lowerNode->mParent != upperNode
		|| footNode->mParent != lowerNode) return;
	auto upper = m_Bone.find(upperName);
	auto lower = m_Bone.find(lowerName);
	auto foot = m_Bone.find(footName);
	if (upper == m_Bone.end() || lower == m_Bone.end() || foot == m_Bone.end()) return;

	const auto origin = aiVector3D(0.0f, 0.0f, 0.0f);
	const aiVector3D hip = upper->second.Matrix * origin;
	const aiVector3D knee = lower->second.Matrix * origin;
	const aiVector3D ankle = foot->second.Matrix * origin;
	const float upperLength = (knee - hip).Length();
	const float lowerLength = (ankle - knee).Length();
	if (upperLength < 0.001f || lowerLength < 0.001f) return;

	aiMatrix4x4 rootInverse = m_AiScene->mRootNode->mTransformation;
	rootInverse.Inverse();
	const aiVector3D ankleModel = rootInverse * ankle;
	const XMMATRIX world = GetWorldMatrix();
	XMFLOAT3 ankleWorld{};
	XMStoreFloat3(&ankleWorld, XMVector3TransformCoord(
		XMVectorSet(ankleModel.x, ankleModel.y, ankleModel.z, 1.0f), world));
	const float groundY = m_GroundHeight(ankleWorld.x, ankleWorld.z)
		+ m_AnkleToSoleHeight[footIndex] - m_FootTargetCorrection[footIndex];
	float ikWeight = std::clamp(m_FootIKStrength, 0.0f, 1.0f);
	if (ikWeight < 1.0f)
	{
		// 走行中は接地脚を完全に合わせ、持ち上げた脚だけスイングを残す。
		const float nearGround = std::clamp(
			(0.28f - (ankleWorld.y - groundY)) / 0.18f, 0.0f, 1.0f);
		ikWeight = nearGround;
	}
	m_FootIKWeight[footIndex] = ikWeight;
	const float targetY = ankleWorld.y + ikWeight
		* (groundY - ankleWorld.y);
	XMFLOAT3 targetModel{};
	XMStoreFloat3(&targetModel, XMVector3TransformCoord(
		XMVectorSet(ankleWorld.x, targetY, ankleWorld.z, 1.0f),
		XMMatrixInverse(nullptr, world)));
	const aiVector3D targetRaw = m_AiScene->mRootNode->mTransformation
		* aiVector3D(targetModel.x, targetModel.y, targetModel.z);
	aiVector3D direction = targetRaw - hip;
	const float unclampedDistance = direction.Length();
	if (unclampedDistance < 0.001f) return;
	direction /= unclampedDistance;
	const float distance = std::clamp(unclampedDistance,
		(std::abs)(upperLength - lowerLength) + 0.001f,
		upperLength + lowerLength - 0.001f);
	const aiVector3D reachableTarget = hip + direction * distance;
	const float along = (upperLength * upperLength - lowerLength * lowerLength
		+ distance * distance) / (2.0f * distance);
	const float sideways = sqrtf((std::max)(0.0f, upperLength * upperLength - along * along));
	aiVector3D bend = knee - hip - direction * ((knee - hip) * direction);
	if (bend.SquareLength() < 0.0001f)
	{
		bend = aiVector3D(0.0f, 0.0f, 1.0f);
		bend -= direction * (bend * direction);
	}
	if (bend.SquareLength() < 0.0001f)
	{
		bend = aiVector3D(1.0f, 0.0f, 0.0f);
		bend -= direction * (bend * direction);
	}
	bend.Normalize();
	const aiVector3D desiredKnee = hip + direction * along + bend * sideways;
	const aiMatrix4x4 originalFootGlobal = foot->second.Matrix;

	auto aimBone = [&](aiNode* node, BONE& bone,
		const aiVector3D& currentDirection, const aiVector3D& wantedDirection)
	{
		aiVector3D from = currentDirection, to = wantedDirection;
		if (from.SquareLength() < 0.0001f || to.SquareLength() < 0.0001f) return;
		from.Normalize();
		to.Normalize();
		aiVector3D axis = from ^ to;
		const float axisLength = axis.Length();
		if (axisLength < 0.00001f) return;
		axis /= axisLength;
		const float angle = atan2f(axisLength, std::clamp(from * to, -1.0f, 1.0f));
		aiMatrix4x4 delta;
		aiMatrix4x4::Rotation(angle, axis, delta);
		auto parent = m_Bone.find(node->mParent->mName.C_Str());
		const aiMatrix4x4 parentGlobal = parent != m_Bone.end()
			? parent->second.Matrix : aiMatrix4x4();
		aiMatrix4x4 parentInverse = parentGlobal;
		parentInverse.Inverse();
		const aiMatrix4x4 oldLocal = bone.AnimationMatrix;
		bone.AnimationMatrix = parentInverse * delta * parentGlobal * oldLocal;
		bone.AnimationMatrix.a4 = oldLocal.a4;
		bone.AnimationMatrix.b4 = oldLocal.b4;
		bone.AnimationMatrix.c4 = oldLocal.c4;
	};

	aimBone(upperNode, upper->second, knee - hip, desiredKnee - hip);
	UpdateBoneMatrix(m_AiScene->mRootNode, aiMatrix4x4());
	const aiVector3D movedKnee = lower->second.Matrix * origin;
	const aiVector3D movedAnkle = foot->second.Matrix * origin;
	aimBone(lowerNode, lower->second, movedAnkle - movedKnee, reachableTarget - movedKnee);
	UpdateBoneMatrix(m_AiScene->mRootNode, aiMatrix4x4());

	// 脚を曲げても靴はアニメーション本来の向きを保つ。
	aiMatrix4x4 lowerInverse = lower->second.Matrix;
	lowerInverse.Inverse();
	const aiMatrix4x4 wantedFootLocal = lowerInverse * originalFootGlobal;
	aiVector3D wantedScale, wantedPosition, currentScale, currentPosition;
	aiQuaternion wantedRotation, currentRotation;
	wantedFootLocal.Decompose(wantedScale, wantedRotation, wantedPosition);
	foot->second.AnimationMatrix.Decompose(currentScale, currentRotation, currentPosition);
	foot->second.AnimationMatrix = aiMatrix4x4(currentScale, wantedRotation, currentPosition);
}

void AnimationModel::BuildSkinTransforms()
{
	aiMatrix4x4 rootInverse = m_AiScene->mRootNode->mTransformation;
	rootInverse.Inverse();
	for (unsigned int meshIndex = 0; meshIndex < m_AiScene->mNumMeshes; ++meshIndex)
	{
		const aiMesh* mesh = m_AiScene->mMeshes[meshIndex];
		auto& transforms = m_SkinTransforms[meshIndex];
		transforms.resize(mesh->mNumBones);
		for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			const aiBone* source = mesh->mBones[boneIndex];
			const auto found = m_Bone.find(source->mName.C_Str());
			transforms[boneIndex] = found != m_Bone.end()
				? rootInverse * found->second.Matrix * source->mOffsetMatrix
				: aiMatrix4x4();
		}
	}
}

void AnimationModel::SampleFootVertices()
{
	if (m_AiScene == nullptr || m_DeformVertex == nullptr) return;
	for (int foot = 0; foot < 2; ++foot)
	{
		m_FootLocalVertices[foot].clear();
		m_FootLocalVertices[foot].reserve(m_FootVertexRefs[foot].size());
		for (const auto& [meshIndex, vertexIndex] : m_FootVertexRefs[foot])
		{
			const DEFORM_VERTEX& source = m_DeformVertex[meshIndex][vertexIndex];
			aiVector3D position{};
			float totalWeight = 0.0f;
			for (int influence = 0; influence < (std::min)(source.BoneNum, 4); ++influence)
			{
				const float weight = source.BoneWeight[influence];
				const int boneIndex = source.BoneIndex[influence];
				if (weight <= 0.0f || boneIndex < 0) continue;
				position += (m_SkinTransforms[meshIndex][boneIndex] * source.Position) * weight;
				totalWeight += weight;
			}
			position = totalWeight > 0.0f ? position / totalWeight : source.Position;
			m_FootLocalVertices[foot].push_back({ position.x, position.y, position.z });
		}
	}
}

void AnimationModel::ApplyAnimation(const aiAnimation* animation, double animationTime,
	float blendWeight, bool uploadMesh)
{
	if (animation == nullptr || m_AiScene == nullptr || m_AiScene->mRootNode == nullptr
		|| m_DeformVertex == nullptr || m_VertexBuffer == nullptr)
	{
		return;
	}
	m_FootLocalVertices[0].clear();
	m_FootLocalVertices[1].clear();

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
	if (blendWeight < 1.0f)
	{
		for (auto& [boneName, bone] : m_Bone)
		{
			const auto source = m_BlendSourcePose.find(boneName);
			if (source == m_BlendSourcePose.end()) continue;
			aiVector3D sourceScale, targetScale, sourcePosition, targetPosition;
			aiQuaternion sourceRotation, targetRotation;
			source->second.Decompose(sourceScale, sourceRotation, sourcePosition);
			bone.AnimationMatrix.Decompose(targetScale, targetRotation, targetPosition);
			aiQuaternion rotation;
			aiQuaternion::Interpolate(rotation, sourceRotation, targetRotation, blendWeight);
			bone.AnimationMatrix = aiMatrix4x4(
				sourceScale + (targetScale - sourceScale) * blendWeight,
				rotation.Normalize(),
				sourcePosition + (targetPosition - sourcePosition) * blendWeight);
		}
	}

	UpdateBoneMatrix(m_AiScene->mRootNode, aiMatrix4x4());
	if (m_GroundHeight != nullptr && m_GameObject != nullptr)
	{
		SolveLegIK("mixamorig:LeftUpLeg", "mixamorig:LeftLeg", "mixamorig:LeftFoot", 0);
		SolveLegIK("mixamorig:RightUpLeg", "mixamorig:RightLeg", "mixamorig:RightFoot", 1);
		UpdateBoneMatrix(m_AiScene->mRootNode, aiMatrix4x4());
	}
	BuildSkinTransforms();
	if (!uploadMesh)
	{
		SampleFootVertices();
		return;
	}

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
		std::vector<aiMatrix3x3> normalTransforms;
		normalTransforms.reserve(m_SkinTransforms[meshIndex].size());
		for (const aiMatrix4x4& transform : m_SkinTransforms[meshIndex])
			normalTransforms.emplace_back(transform);
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
				const int boneIndex = source.BoneIndex[influenceIndex];
				if (weight <= 0.0f || boneIndex < 0)
				{
					continue;
				}
				const aiMatrix4x4& transform = m_SkinTransforms[meshIndex][boneIndex];
				position += (transform * source.Position) * weight;
				normal += (normalTransforms[boneIndex] * source.Normal) * weight;
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
			bool belongsToFoot[2]{};
			for (int influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex)
			{
				if (source.BoneWeight[influenceIndex] < 0.25f) continue;
				const std::string& name = source.BoneName[influenceIndex];
				const int footIndex = name.find("LeftFoot") != std::string::npos
					|| name.find("LeftToe") != std::string::npos ? 0
					: name.find("RightFoot") != std::string::npos
					|| name.find("RightToe") != std::string::npos ? 1 : -1;
				if (footIndex >= 0) belongsToFoot[footIndex] = true;
			}
			for (int footIndex = 0; footIndex < 2; ++footIndex)
			{
				if (belongsToFoot[footIndex])
					m_FootLocalVertices[footIndex].push_back({ position.x, position.y, position.z });
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
	// 足首ボーンと靴底の距離をメッシュから測る。モデルごとのサイズ差に対応する。
	const XMMATRIX world = GetWorldMatrix();
	for (int footIndex = 0; footIndex < 2; ++footIndex)
	{
		if (m_FootLocalVertices[footIndex].empty()) continue;
		const auto foot = m_Bone.find(footIndex == 0
			? "mixamorig:LeftFoot" : "mixamorig:RightFoot");
		if (foot == m_Bone.end()) continue;
		const aiVector3D ankleLocal = globalInverse
			* (foot->second.Matrix * aiVector3D(0.0f, 0.0f, 0.0f));
		XMFLOAT3 ankleWorld{};
		XMStoreFloat3(&ankleWorld, XMVector3TransformCoord(
			XMVectorSet(ankleLocal.x, ankleLocal.y, ankleLocal.z, 1.0f), world));
		float soleY = ankleWorld.y;
		for (const Vector3& local : m_FootLocalVertices[footIndex])
		{
			XMFLOAT3 point{};
			XMStoreFloat3(&point, XMVector3TransformCoord(
				XMVectorSet(local.x, local.y, local.z, 1.0f), world));
			soleY = (std::min)(soleY, point.y);
		}
		const float measured = std::clamp(ankleWorld.y - soleY, 0.03f, 0.35f);
		m_AnkleToSoleHeight[footIndex] = m_AnkleToSoleHeight[footIndex] * 0.8f
			+ measured * 0.2f;
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

