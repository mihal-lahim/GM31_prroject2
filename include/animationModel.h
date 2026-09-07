#pragma once

#include <string>
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
	std::unordered_map<std::string, BONE> m_Bone;//ボーンデータ（名前で参照）
	float m_ModelScale = 1.0f;
	float m_ModelYawOffset = 0.0f;
	bool m_Visible = true;

	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Matrix);

public:
	using Component::Component;

	void Load( const char *FileName );
	void LoadAnimation( const char *FileName, const char *Name );
	void SetModelScale(float Scale) { m_ModelScale = Scale; }
	void SetModelYawOffset(float YawOffset) { m_ModelYawOffset = YawOffset; }
	// 被弾中の点滅など、更新は続けたまま描画だけを止めたい場合に使います。
	void SetVisible(bool Visible) { m_Visible = Visible; }
	bool IsVisible() const { return m_Visible; }
	void Uninit() override;
	void Update(const char* AnimationName1, int Frame1);
	void Draw() override;
};
