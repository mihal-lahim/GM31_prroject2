#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "assimp-vc143-mt.lib")

#include "component.h"
#include "engineCommon.h"


//変形後頂点構造体
struct DEFORM_VERTEX
{
	aiVector3D Position;
	aiVector3D Normal;
	int				BoneNum;
	std::string		BoneName[4];//本来はボーンインデックスで管理するべき
	float			BoneWeight[4];
	int			BoneIndex[4];
	aiMatrix4x4	BoneOffsetMatrix[4];//オフセットはメッシュごとに異なるため頂点側で保持
};

//ボーン構造体
struct BONE
{
	aiMatrix4x4 Matrix;
	aiMatrix4x4 AnimationMatrix;
};

class AnimationModel : public Component
{
private:
	const aiScene* m_AiScene = nullptr;
	std::unordered_map<std::string, const aiScene*> m_Animation;

	ID3D11Buffer**	m_VertexBuffer = nullptr;
	ID3D11Buffer**	m_IndexBuffer = nullptr;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;

	std::vector<DEFORM_VERTEX>* m_DeformVertex = nullptr;//変形後頂点データ
	std::vector<std::vector<aiMatrix4x4>> m_SkinTransforms;
	std::unordered_map<std::string, BONE> m_Bone;//ボーンデータ（名前で参照）
	float m_ModelScale = 1.0f;
	float m_ModelYawOffset = 0.0f;
	bool m_Visible = true;
	float (*m_GroundHeight)(float, float) = nullptr;
	float m_FootIKStrength = 1.0f;
	float m_AnkleToSoleHeight[2]{ 0.12f, 0.12f };
	float m_FootTargetCorrection[2]{};
	float m_FootIKWeight[2]{};
	std::vector<Vector3> m_FootLocalVertices[2];
	std::vector<std::pair<unsigned int, unsigned int>> m_FootVertexRefs[2];
	std::string m_CurrentAnimation;
	std::unordered_map<std::string, aiMatrix4x4> m_BlendSourcePose;
	float m_AnimationSeconds = 0.0f;
	float m_BlendSeconds = 0.0f;
	float m_BlendDuration = 0.0f;

	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Matrix);
	void ApplyAnimation(const aiAnimation* animation, double animationTime, float blendWeight,
		bool uploadMesh);
	void SampleFootVertices();
	void BuildSkinTransforms();
	XMMATRIX GetWorldMatrix() const;
	void SolveLegIK(const char* upperName, const char* lowerName, const char* footName, int footIndex);

public:
	using Component::Component;

	void Load( const char *FileName );
	void LoadAnimation( const char *FileName, const char *Name );
	void SetModelScale(float Scale) { m_ModelScale = Scale; }
	void SetModelYawOffset(float YawOffset) { m_ModelYawOffset = YawOffset; }
	void SetFootIK(float (*heightAt)(float, float), float strength = 1.0f)
	{
		m_GroundHeight = heightAt;
		m_FootIKStrength = strength;
	}
	// 被弾中の点滅など、更新は続けたまま描画だけを止めたい場合に使います。
	void SetVisible(bool Visible) { m_Visible = Visible; }
	bool IsVisible() const { return m_Visible; }
	void Uninit() override;
	// 切り替え時の現在姿勢から、指定秒数で新しいクリップへ移行する。
	void PlayAnimation(const char* name, float blendSeconds = 0.2f);
	void TickAnimation(float deltaSeconds, bool uploadMesh = true);
	// 直近の足メッシュ全頂点と地面を比較し、埋まらないために必要な高さを返す。
	bool GetRequiredGroundOffset(float (*heightAt)(float, float), float& offset) const;
	// 各足で地面に最も近いメッシュ頂点の高さ。正なら足が浮いている。
	bool GetFootClearance(float (*heightAt)(float, float), float (&clearance)[2]) const;
	void ResetFootTargetCorrection() { m_FootTargetCorrection[0] = m_FootTargetCorrection[1] = 0.0f; }
	void AdjustFootTargetCorrection(int footIndex, float clearance)
	{
		if (footIndex >= 0 && footIndex < 2)
			m_FootTargetCorrection[footIndex] = std::clamp(
				m_FootTargetCorrection[footIndex] + clearance, -0.8f, 0.8f);
	}
	float GetFootIKWeight(int footIndex) const { return m_FootIKWeight[footIndex]; }
	void Update(const char* AnimationName1, int Frame1);
	void Draw() override;
};
