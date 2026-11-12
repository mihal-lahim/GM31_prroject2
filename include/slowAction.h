#pragma once

#include "scene.h"
#include "gameObject.h"
#include "cameraView.h"
#include "collisionWorld.h"

class Collider;
class DynamicText;
class ModelRenderer;

namespace SlowCollisionLayers
{
	inline constexpr CollisionLayer World = 1u << 1;
	inline constexpr CollisionLayer Player = 1u << 2;
	inline constexpr CollisionLayer Enemy = 1u << 3;
}

// 1ステップ分の操作入力です。キーボードとマウスから作りますが、
// レンダーテストからは同じ形で差し替えられます。
struct SlowInputState
{
	// カメラ基準の移動入力です。Xが右、Zが前で、それぞれ-1〜1です。
	float MoveX = 0.0f;
	float MoveZ = 0.0f;
	bool Dash = false;
	bool Attack = false;
	bool Slow = false;
};

class SlowInput
{
public:
	// シーンが1ステップに1回だけ呼びます。以降そのステップ中は同じ値を返します。
	static void Poll();
	static const SlowInputState& Get();
	// nullptr以外を渡している間、実デバイスの代わりにその値を使います(テスト用)。
	static void SetOverride(const SlowInputState* state);
};

// 実時間とワールド時間を分ける時計です。時間減速ゲージもここで持ちます。
//   実時間     GameTime::GetDeltaTime()            プレイヤー・カメラ・UI
//   ワールド時間 SlowClock::GetWorldDeltaTime()      敵の移動・AI・攻撃・吹き飛び
class SlowClock
{
public:
	static void Reset();
	// シーンが1ステップに1回、オブジェクトの更新より前に呼びます。
	static void Update(float realDeltaTime, bool slowHeld);

	static float GetWorldScale();
	static float GetWorldDeltaTime();
	static bool IsSlowActive();
	static float GetGauge();
	// 減速中に獲得し、まだゲージへ反映していない回復量です。
	static float GetPendingGain();
	// 減速中なら蓄積し、減速していなければその場で反映します。
	static void AddGauge(float amount);
};

class SlowCamera final : public GameObject, public ICameraView
{
public:
	void Init() override;
	void PreUpdate() override;
	void LateUpdate() override;
	void Draw() override;

	XMMATRIX GetViewMatrix() const override { return m_View; }
	Vector3 GetCameraPosition() const override { return m_Eye; }
	Vector3 GetCameraForward() const override;

	float GetYaw() const { return m_Yaw; }
	void SetYaw(float yaw) { m_Yaw = yaw; }
	float GetPitch() const { return m_Pitch; }
	float GetFovDegrees() const { return m_FovDegrees; }
	float GetDistance() const { return m_Distance; }
	// 指定したワールド座標の方へ、向きを少しだけ寄せます。マウスを動かすと中断します。
	void AssistToward(const Vector3& worldPoint);
	// 追従の遅れをなくして、今のプレイヤー位置へ合わせます。
	void SnapToTarget();

private:
	XMMATRIX m_View = XMMatrixIdentity();
	Vector3 m_Focus{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Eye{ 0.0f, 0.0f, 0.0f };
	float m_Yaw = 0.0f;
	// 注視点から見たカメラの仰角です。大きいほど上から見下ろします。
	float m_Pitch = 0.0f;
	float m_Distance = 0.0f;
	float m_FovDegrees = 60.0f;
	float m_AssistRemaining = 0.0f;
	bool m_HasFocus = false;
};

class SlowPlayer final : public GameObject
{
public:
	enum class State
	{
		Move,
		Dash,
		Attack
	};

	enum class AttackType
	{
		None,
		Normal,
		Strong
	};

	void Init() override;
	void Update() override;
	// 配置された向きを、移動で使う向き(m_Yaw)へ取り込みます。
	void OnPropertiesChanged() override { m_Yaw = m_Rotation.y; }

	void TakeDamage(int damage);

	State GetState() const { return m_State; }
	AttackType GetAttackType() const { return m_State == State::Attack ? m_AttackType : AttackType::None; }
	AttackType GetBufferedAttack() const { return m_BufferedAttack; }
	bool IsDashLocked() const { return m_DashLocked; }
	// 左クリックを押し続けている秒数です。押していなければ0です。
	float GetAttackHoldSeconds() const { return m_AttackHoldSeconds; }
	int GetHp() const { return m_Hp; }
	float GetYaw() const { return m_Yaw; }
	void SetYaw(float yaw) { m_Yaw = yaw; m_Rotation.y = yaw; }
	float GetSpeed() const { return m_Velocity.length(); }
	int GetPursuitCount() const { return m_PursuitCount; }

private:
	Collider* m_Collider = nullptr;
	State m_State = State::Move;
	AttackType m_AttackType = AttackType::None;
	AttackType m_BufferedAttack = AttackType::None;
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	Vector3 m_AttackDirection{ 0.0f, 0.0f, 1.0f };
	float m_Yaw = 0.0f;
	float m_AttackTime = 0.0f;
	float m_AttackHoldSeconds = 0.0f;
	bool m_AttackHitDone = false;
	// ダッシュから攻撃へ移ったとき、Shiftを一度離すまで再ダッシュを止めるためのフラグです。
	bool m_DashLocked = false;
	bool m_PreviousDash = false;
	bool m_PreviousAttack = false;
	int m_Hp = 0;
	int m_PursuitCount = 0;

	void StartAttack(AttackType type, bool dashHeld);
	void PerformAttackHit();
	void UpdateMovement(const SlowInputState& input, float deltaTime);
	bool TryMove(const Vector3& offset);
};

class SlowEnemy final : public GameObject
{
public:
	enum class State
	{
		Idle,
		Chase,
		Windup,
		Cooldown,
		Stagger,
		Launched,
		Recover
	};

	struct HitResult
	{
		// この攻撃で吹き飛んだ(または吹き飛び直した)か。
		bool Launched = false;
		// 吹き飛び中に当てた追撃か。
		bool Pursuit = false;
		bool Killed = false;
	};

	void Init() override;
	void Update() override;
	void Reflect(Engine::PropertyVisitor& visitor) override;
	void OnPropertiesChanged() override;

	// directionは水平の攻撃方向です。吹き飛ばす場合はこの向きへ飛びます。
	HitResult ApplyHit(const Vector3& direction, bool strong, int damage);

	State GetState() const { return m_State; }
	int GetHp() const { return m_Hp; }
	bool IsLaunched() const { return m_State == State::Launched; }
	// 着地までの残り時間(ワールド時間)です。吹き飛び中でなければ0です。
	float GetLaunchRemaining() const;
	int GetWallBounceCount() const { return m_WallBounces; }
	bool IsHanging() const { return m_State == State::Launched && m_HangTime > 0.0f; }

	// trueの間、敵のHPが減りません(吹き飛ばしと追撃の練習用)。
	static void SetInvincible(bool enabled) { s_Invincible = enabled; }
	static bool IsInvincible() { return s_Invincible; }

private:
	static bool s_Invincible;

	Collider* m_Collider = nullptr;
	ModelRenderer* m_NormalModel = nullptr;
	ModelRenderer* m_WindupModel = nullptr;
	ModelRenderer* m_LaunchedModel = nullptr;
	ModelRenderer* m_DownModel = nullptr;
	State m_State = State::Idle;
	float m_StateTime = 0.0f;
	int m_Hp = 0;
	// 1以上なら、この個体だけ調整値(EnemyHp)の代わりにこのHPで始めます。
	int m_HpOverride = 0;
	Vector3 m_LaunchDirection{ 0.0f, 0.0f, 1.0f };
	float m_LaunchTime = 0.0f;
	float m_LaunchGravity = 0.0f;
	float m_VerticalSpeed = 0.0f;
	float m_HangTime = 0.0f;
	int m_WallBounces = 0;
	Vector3 m_StaggerVelocity{ 0.0f, 0.0f, 0.0f };

	void SetState(State state, float seconds = 0.0f);
	void Launch(const Vector3& direction);
	void UpdateLaunched(float worldDeltaTime);
	void UpdateChase(float worldDeltaTime);
	void Strike();
	void FaceTowards(const Vector3& direction);
	bool TryMove(const Vector3& offset);
	void RefreshModel();
};

// 壁と障害物です。scaleがそのまま半サイズになります。
class SlowBlock final : public GameObject
{
public:
	void Init() override;
	void Reflect(Engine::PropertyVisitor& visitor) override;
	void OnPropertiesChanged() override;

private:
	// trueなら反射テスト用の障害物の色、falseなら外周の壁の色で描きます。当たり判定は同じです。
	bool m_Obstacle = false;
	ModelRenderer* m_WallModel = nullptr;
	ModelRenderer* m_ObstacleModel = nullptr;
};

// 市松模様の床です。ステージは平らなので見た目だけで、当たり判定は持ちません。
class SlowFloor final : public GameObject
{
public:
	void Init() override;
};

class SlowActionScene final : public Scene
{
public:
	// 外周の壁の内側の半サイズです。
	static constexpr float kArenaHalfSize = 20.0f;

	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override {}

	// F3と同じ切り替えです。テストからも呼びます。
	void ToggleEditMode();

private:
	DynamicText* m_Status = nullptr;
	DynamicText* m_EnemyInfo = nullptr;
	DynamicText* m_Help = nullptr;
	float m_RespawnTimer = 0.0f;

	void UpdateHud();
	void RespawnEnemies();
};
