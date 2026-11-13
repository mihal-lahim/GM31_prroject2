#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "assimp-vc143-mt.lib")

#include "animator.h"
#include "component.h"
#include "engineCommon.h"
#include "surfaceMaterial.h"


//変形後頂点構造体
struct DEFORM_VERTEX
{
	aiVector3D Position;
	aiVector3D Normal;
	int				BoneNum;
	float			BoneWeight[4];
	int			BoneIndex[4];
	// 各影響ボーンが左足(0)・右足(1)・それ以外(-1)のどれかを読み込み時に確定させた値です。
	// 以前は頂点ごとにボーン名の文字列を4本持ち、毎フレーム全頂点でfindしていました。
	signed char	BoneFoot[4];
};

//ボーン構造体
struct BONE
{
	aiMatrix4x4 Matrix;
	aiMatrix4x4 AnimationMatrix;
};

// 読み込み済みaiSceneの共有ハンドルです。同じFBXを使う複数のAnimationModelが1つを共有します。
using AnimationScenePtr = std::shared_ptr<const aiScene>;

class AnimationModel : public Component
{
private:
	AnimationScenePtr m_AiScene;
	std::unordered_map<std::string, AnimationScenePtr> m_Animation;

	// メッシュごとのGPUバッファです。要素数はm_AiScene->mNumMeshesと一致します。
	std::vector<ComPtr<ID3D11Buffer>> m_VertexBuffer;
	std::vector<ComPtr<ID3D11Buffer>> m_IndexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	Engine::SurfaceMaterial m_Surface;
	bool m_UseSurfaceShader = false;

	std::unordered_map<std::string, ComPtr<ID3D11ShaderResourceView>> m_Texture;

	std::vector<std::vector<DEFORM_VERTEX>> m_DeformVertex;//変形後頂点データ(メッシュごと)
	std::vector<std::vector<aiMatrix4x4>> m_SkinTransforms;
	std::unordered_map<std::string, BONE> m_Bone;//ボーンデータ（名前で参照）
	float m_ModelScale = 1.0f;
	float m_ModelYawOffset = 0.0f;
	float m_VerticalOffset = 0.0f;
	bool m_Visible = true;
	float (*m_GroundHeight)(float, float) = nullptr;
	Vector3 (*m_GroundNormal)(float, float) = nullptr;
	float m_FootIKStrength = 1.0f;
	float m_FootIKFade = 1.0f;
	float m_AnkleToSoleHeight[2]{ 0.12f, 0.12f };
	float m_FootTargetCorrection[2]{};
	float m_FootIKWeight[2]{};
	std::vector<Vector3> m_FootLocalVertices[2];
	std::vector<std::pair<unsigned int, unsigned int>> m_FootVertexRefs[2];
	// クリップの選択・ブレンドツリー・アクション層の合成はAnimatorが行います(描画に依存しない部分)。
	Anim::Animator m_Animator;
	// Animatorのボーン番号 → m_Boneの要素。毎フレームの名前検索を避けます。
	std::vector<BONE*> m_PoseBones;
	// Pose before terrain IK, so a transition never applies the old IK twice.
	std::unordered_map<std::string, aiMatrix4x4> m_AnimatedPose;

	void CreateBone(aiNode* Node);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Matrix);
	void ApplyPose(const Anim::Pose& pose, bool uploadMesh);
	void SampleFootVertices();
	void BuildSkinTransforms();
	XMMATRIX GetWorldMatrix() const;
	void SolveLegIK(const char* upperName, const char* lowerName, const char* footName, int footIndex);

public:
	using Component::Component;

	void Load( const char *FileName );
	void LoadAnimation( const char *FileName, const char *Name );
	// 変形済みの頂点バッファをそのまま使って、太陽から見た深度を描きます。
	void DrawShadow() override;
	void SetModelScale(float Scale) { m_ModelScale = Scale; }
	void SetModelYawOffset(float YawOffset) { m_ModelYawOffset = YawOffset; }
	void SetVerticalOffset(float offset) { m_VerticalOffset = offset; }
	bool GetBoneWorldPosition(const char* name, Vector3& position) const;
	bool SetShaderDefinition(std::string_view id);
	bool SetShaderFloat(std::string_view name, float value);
	bool SetShaderTexture(std::string_view name, const std::filesystem::path& path);
	bool SetShaderMode(Engine::SurfaceShader shader);
	void UseDefaultShader() { m_UseSurfaceShader = false; }
	void SetFootIK(float (*heightAt)(float, float), float strength = 1.0f,
		Vector3 (*normalAt)(float, float) = nullptr)
	{
		m_GroundHeight = heightAt;
		m_GroundNormal = normalAt;
		m_FootIKStrength = strength;
	}
	void SetFootIKFade(float weight) { m_FootIKFade = std::clamp(weight, 0.0f, 1.0f); }
	// 被弾中の点滅など、更新は続けたまま描画だけを止めたい場合に使います。
	void SetVisible(bool Visible) { m_Visible = Visible; }
	bool IsVisible() const { return m_Visible; }
	void Uninit() override;
	// 切り替え時の現在姿勢から、指定秒数で新しいクリップへ移行する。
	// nameにはクリップ名か、AddBlendSpace1Dで登録したブレンドツリー名を指定できます。
	void PlayAnimation(const char* name, float blendSeconds = 0.2f);
	// Clip-specific playback range as normalized fractions of the source clip.
	// 単体クリップ用です。ブレンドツリーのクリップは各BlendClip::Playbackを使います。
	void SetPlayback(float speed, float startFraction = 0.0f, float endFraction = 1.0f,
		bool loop = true)
	{
		m_Animator.SetPlayback({ speed, startFraction, endFraction, loop });
	}
	void TickAnimation(float deltaSeconds, bool uploadMesh = true);
	const auto& GetAnimationPose() const { return m_AnimatedPose; }
	const std::string& GetAnimationName() const { return m_Animator.GetCurrentName(); }

	// ブレンドツリー・アクション層 (仕様は ANIMATION.md) ------------------------------
	// parameterの値でclipsを混ぜる1Dブレンドツリーを登録します。PlayAnimation(name)で再生します。
	void AddBlendSpace1D(const char* name, const char* parameter,
		std::vector<Anim::BlendClip> clips, float parameterSmoothing = 0.1f);
	void SetParameter(const char* name, float value) { if (name != nullptr) m_Animator.SetParameter(name, value); }
	// 例: {{"mixamorig:Spine", 0.4f}, {"mixamorig:Spine1", 0.75f}, {"mixamorig:Spine2", 1.0f}}
	void DefineBoneMask(const char* name, const std::vector<std::pair<std::string, float>>& bones);
	// 攻撃などの単発モーションを、ベース層(移動など)の上に重ねて再生します。
	bool PlayAction(const char* clip, const Anim::ActionOptions& options = {});
	void StopAction(int layer = 0, float fadeOutSeconds = -1.0f) { m_Animator.StopAction(layer, fadeOutSeconds); }
	bool IsActionActive(int layer = 0) const { return m_Animator.IsActionActive(layer); }
	float GetActionTime(int layer = 0) const { return m_Animator.GetActionTime(layer); }
	bool ActionPassed(float time, int layer = 0) const { return m_Animator.ActionPassed(time, layer); }
	void SetActionMaskStrength(float strength, int layer = 0, float smoothingSeconds = 0.15f)
	{
		m_Animator.SetActionMaskStrength(strength, layer, smoothingSeconds);
	}
	// 層全体の重み(0〜1)。加算の傾き・呼吸などの効き具合をゲーム側で決めるときに使います。
	void SetLayerWeight(float weight, int layer = 0) { m_Animator.SetLayerWeight(weight, layer); }
	// RootMotion::Extract のアクションが直前のTickAnimationで動かした量(ワールド座標・水平のみ)。
	// キャラクターの位置へ足すと、踏み込み・回避の移動がモーションの足運びと一致します。
	Vector3 GetRootMotionDelta() const;
	Anim::Animator& GetAnimator() { return m_Animator; }
	const Anim::Animator& GetAnimator() const { return m_Animator; }
	// 直近の足メッシュ全頂点と地面を比較し、埋まらないために必要な高さを返す。
	bool GetRequiredGroundOffset(float (*heightAt)(float, float), float& offset) const;
	// 各足で地面に最も近いメッシュ頂点の高さ。正なら足が浮いている。
	bool GetFootClearance(float (*heightAt)(float, float), float (&clearance)[2]) const;
	// 靴の前後それぞれで、地面に最も近い頂点との距離を測る。
	bool GetFootEndClearance(float (*heightAt)(float, float), float (&clearance)[2][2]) const;
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
