#pragma once

#include "scene.h"
#include "gameObject.h"
#include "cameraView.h"
#include "collisionWorld.h"

class Collider;
class DynamicText;
class ModelRenderer;
class Audio;

namespace FpsCollisionLayers
{
	inline constexpr CollisionLayer World = 1u << 1;
	inline constexpr CollisionLayer Player = 1u << 2;
	inline constexpr CollisionLayer Target = 1u << 3;
}

// 一人称のプレイヤーです。カメラも兼ねるので、描画はCameraレイヤーで最初に行います。
class FpsPlayer final : public GameObject, public ICameraView
{
public:
	static constexpr float kEyeHeight = 1.6f;

	void Init() override;
	void PreUpdate() override;
	void Update() override;
	void LateUpdate() override;
	void Draw() override;

	XMMATRIX GetViewMatrix() const override { return m_View; }
	Vector3 GetCameraPosition() const override;
	// 入力を時間で補間してからsmoothstepで加減速します。解除・押し直しも現在位置から続きます。
	float GetAimBlend() const { return m_AimProgress * m_AimProgress * (3.0f - 2.0f * m_AimProgress); }
	Vector3 GetCameraForward() const override;

	int GetScore() const { return m_Score; }
	// 武器の揺れに使う、直近フレームの水平移動の速さ(m/s)です。
	float GetMoveSpeed() const { return m_MoveSpeed; }
	bool IsGrounded() const { return m_Grounded; }
	// 視線の先へ1発撃ちます。通常は左クリックで呼ばれ、レンダーテストからも直接呼びます。
	void Shoot();

private:
	Collider* m_Collider = nullptr;
	XMMATRIX m_View = XMMatrixIdentity();
	float m_Yaw = 0.0f;
	float m_Pitch = 0.0f;
	float m_VerticalSpeed = 0.0f;
	float m_MoveSpeed = 0.0f;
	float m_FireCooldown = 0.0f;
	float m_AimProgress = 0.0f;
	bool m_Grounded = false;
	int m_Score = 0;

	void Move(float deltaTime);
	bool TryMoveHorizontal(const Vector3& offset);
};

// 画面の手元に描く銃です。ワールドを描き終えた後に深度を消してから描くので、壁にめり込みません。
class FpsWeapon final : public GameObject
{
public:
	void Init() override;
	void Update() override;
	void Draw() override;
	// 撃った瞬間の反動・マズルフラッシュ・発射音です。
	void Kick();
	// 的を壊したときの音です。
	void PlayTargetBreakSound();
	// マズルフラッシュを出している間だけtrueを返し、炎を置くワールド行列を渡します。
	bool GetMuzzleFlash(XMMATRIX& world) const;

private:
	// 発射音は1秒余りあるので、連射(0.12秒間隔)で尾が切れすぎないよう多めに持ちます。
	static constexpr int kShotVoices = 8;

	ModelRenderer* m_Model = nullptr;
	Audio* m_ShotVoices[kShotVoices]{};
	Audio* m_TargetBreakVoice = nullptr;
	int m_NextShotVoice = 0;
	XMMATRIX m_GunWorld = XMMatrixIdentity();
	float m_Recoil = 0.0f;
	float m_BobTime = 0.0f;
	float m_BobWeight = 0.0f;
	float m_FlashTime = 0.0f;
	float m_FlashRoll = 0.0f;
	float m_FlashScale = 1.0f;
};

// 銃口の炎です。銃の後に描き、加算合成で光らせます。
class FpsMuzzleFlash final : public GameObject
{
public:
	void Init() override;
	void Draw() override;

private:
	ModelRenderer* m_Model = nullptr;
};

// 箱でステージを組むための静的ブロックです。scaleがそのまま半サイズになります。
class FpsBlock final : public GameObject
{
public:
	void Init() override;
};

// 撃つと消える的です。
class FpsTarget final : public GameObject
{
public:
	void Init() override;
	void Update() override;
};

// 命中位置に4秒間残る小さな標的模様。コライダーは持たず、次の射撃を遮りません。
class FpsImpactMarker final : public GameObject
{
public:
	void Init() override;
	void Update() override;
	void Draw() override;
	void Place(const Vector3& point, const Vector3& normal);

private:
	XMMATRIX m_SurfaceRotation = XMMatrixIdentity();
	float m_RemainingSeconds = 4.0f;
};

class FpsGameScene final : public Scene
{
public:
	void Init() override;
	void Uninit() override {}
	void Update() override;
	void Draw() override {}

private:
	DynamicText* m_Status = nullptr;
	float m_ElapsedSeconds = 0.0f;
	bool m_Cleared = false;
};
