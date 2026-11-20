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

	// 埋め込みテクスチャです。同じFBXを使うモデルどうしで共有します(1体ごとにPNGを解凍し直すと、出現のたびに止まるため)。
	std::shared_ptr<const std::unordered_map<std::string, ComPtr<ID3D11ShaderResourceView>>> m_Texture;

	std::vector<std::vector<DEFORM_VERTEX>> m_DeformVertex;//変形後頂点データ(メッシュごと)
	std::vector<std::vector<aiMatrix4x4>> m_SkinTransforms;
	std::unordered_map<std::string, BONE> m_Bone;//ボーンデータ（名前で参照）
	float m_ModelScale = 1.0f;
	float m_ModelYawOffset = 0.0f;
	// モデルの中での傾き(ラジアン)です。向きより先に掛けるので、進行方向に対する左右(ロール)と前後(ピッチ)になります。
	float m_LeanRoll = 0.0f;
	float m_LeanPitch = 0.0f;
	float m_VerticalOffset = 0.0f;
	bool m_Visible = true;
	XMFLOAT4 m_Tint{ 1.0f, 1.0f, 1.0f, 1.0f };
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
	// 埋め込みテクスチャの表(ファイル内の名前 → テクスチャ)です。
	using TextureTable = std::unordered_map<std::string, ComPtr<ID3D11ShaderResourceView>>;

	void Load( const char *FileName );
	// Loadでモデルを読み込めたか。読めなかったモデルは描かれず、アニメーションも動きません。
	bool IsLoaded() const { return m_AiScene != nullptr; }
	void LoadAnimation( const char *FileName, const char *Name );
	// 腰が前へ進んでいくクリップ(In Placeでない走りなど)を、その場で再生するようにします。LoadAnimationの後に呼びます。
	void SetAnimationInPlace(const char* name, bool inPlace = true) { if (name != nullptr) m_Animator.SetClipInPlace(name, inPlace); }
	// 別の体格のキャラクター用に作られたクリップを、このモデルの骨の長さのまま再生します(手足が伸びるのを防ぎます)。
	void SetAnimationKeepProportions(const char* name, bool keep = true) { if (name != nullptr) m_Animator.SetClipKeepProportions(name, keep); }
	// 読み込んだクリップ全体の長さ(秒、再生倍率1)です。無ければ0です。
	float GetAnimationSeconds(const char* name) const { return name != nullptr ? m_Animator.GetClipSeconds(name) : 0.0f; }
	// 変形済みの頂点バッファをそのまま使って、太陽から見た深度を描きます。
	void DrawShadow() override;
	void SetModelScale(float Scale) { m_ModelScale = Scale; }
	void SetModelYawOffset(float YawOffset) { m_ModelYawOffset = YawOffset; }
	// 体を進行方向に対して傾けます(ラジアン)。roll=左右、pitch=前後。足元(原点)を中心に傾き、コライダーは変わりません。
	void SetBodyLean(float roll, float pitch) { m_LeanRoll = roll; m_LeanPitch = pitch; }
	float GetBodyRoll() const { return m_LeanRoll; }
	float GetBodyPitch() const { return m_LeanPitch; }
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
	// マテリアルの色に掛ける色です。被弾の光や予兆の色付けなど、状態を色で見せるときに使います。1以上で明るくなります。
	void SetTint(const XMFLOAT4& tint) { m_Tint = tint; }
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

	// ブレンドツリー・アクション層 (仕様は docs/animation.md) ------------------------------
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
	// 再生中のアクションの再生倍率を途中で変えます(攻撃の緩急など)。位相は保たれます。
	void SetActionSpeed(float speed, int layer = 0) { m_Animator.SetActionSpeed(speed, layer); }
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
