#pragma once

#include "scene.h"
#include "gameObject.h"
#include "cameraView.h"
#include "collisionWorld.h"

class AnimationModel;
class Audio;
class Collider;
class DynamicText;
class ModelRenderer;
class SlowEnemy;

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
	// 減速中でもその場でゲージへ反映します(ジャスト減速の回復用)。
	static void AddGaugeNow(float amount);
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
	// 画面を揺らします。今の揺れより強いときだけ上書きし、実時間で収まります。
	void AddShake(float strength) { m_Shake = (std::max)(m_Shake, strength); }
	float GetShake() const { return m_Shake; }
	// 追従の遅れをなくして、今のプレイヤー位置へ合わせます。
	void SnapToTarget();
	// worldにカメラがあれば揺らします。
	static void Shake(World& world, float strength);

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
	float m_Shake = 0.0f;
	float m_ShakeTime = 0.0f;
	bool m_HasFocus = false;
};

class SlowPlayer final : public GameObject
{
public:
	enum class State
	{
		Move,
		Dash,
		Attack,
		// 左クリックを押し続けて強攻撃を溜めている間です。構えたまま動けません。
		Charge
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
	// HPを指定した値にします(0〜PlayerMaxHpに収めます)。回復とテスト用です。
	void SetHp(int hp);
	float GetYaw() const { return m_Yaw; }
	void SetYaw(float yaw) { m_Yaw = yaw; m_Rotation.y = yaw; }
	float GetSpeed() const { return m_Velocity.length(); }
	int GetPursuitCount() const { return m_PursuitCount; }
	// 通常攻撃の何段目か(1〜3)。通常攻撃中でなければ0です。
	int GetComboStep() const { return m_State == State::Attack && m_AttackType == AttackType::Normal ? m_ComboStep : 0; }
	// 今の攻撃の長さ(秒)です。通常攻撃は段ごとにクリップの区間から決まります。
	float GetAttackDuration() const { return m_AttackDuration; }
	float GetAttackHitDelay() const { return m_AttackHitDelay; }
	// 剣が右手の骨に付いて描かれているか(骨が見つからなければfalseで、剣は隠れます)。
	bool IsSwordAttached() const { return m_SwordAttached; }
	// 再生中のベース層のクリップ名(Idle/Run/Dash)と腰の位置です。テストと確認用です。
	const std::string& GetAnimationName() const;
	bool GetHipsWorldPosition(Vector3& position) const;

private:
	Collider* m_Collider = nullptr;
	AnimationModel* m_Model = nullptr;
	// 右手に持たせる大剣です。毎フレーム手の骨からワールド行列を作って描きます。
	ModelRenderer* m_Sword = nullptr;
	bool m_SwordAttached = false;
	State m_State = State::Move;
	AttackType m_AttackType = AttackType::None;
	AttackType m_BufferedAttack = AttackType::None;
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	Vector3 m_AttackDirection{ 0.0f, 0.0f, 1.0f };
	float m_Yaw = 0.0f;
	float m_AttackTime = 0.0f;
	float m_AttackDuration = 0.0f;
	float m_AttackHitDelay = 0.0f;
	float m_AttackHoldSeconds = 0.0f;
	bool m_AttackHitDone = false;
	// 最後に出した通常攻撃の段(1〜3)。強攻撃・ダッシュ・猶予切れで0に戻ります。
	int m_ComboStep = 0;
	// 攻撃が終わってからの秒数です。ComboChainSeconds以内なら次の段へつながります。
	float m_ComboChainTimer = 0.0f;
	// ダッシュから攻撃へ移ったとき、Shiftを一度離すまで再ダッシュを止めるためのフラグです。
	bool m_DashLocked = false;
	bool m_PreviousDash = false;
	bool m_PreviousAttack = false;
	int m_Hp = 0;
	int m_PursuitCount = 0;
	// 通常攻撃でロックした敵と、当たる瞬間までに残っている踏み込み距離です。
	SlowEnemy* m_AttackTarget = nullptr;
	float m_LungeRemaining = 0.0f;
	float m_LungeSpeed = 0.0f;

	void StartCharge(const SlowInputState& input);
	void UpdateCharge(const SlowInputState& input, float deltaTime);
	void StartAttack(AttackType type, const SlowInputState& input);
	// 通常攻撃のロック先へ向きを追わせ、踏み込みます。当たる瞬間までだけ動きます。
	void UpdateAttackLunge(float deltaTime);
	void PerformAttackHit();
	void UpdateMovement(const SlowInputState& input, float deltaTime);
	bool TryMove(const Vector3& offset);
	// 状態と速さに合わせてアニメーションを選び、実時間で進めます。
	void UpdateAnimation(float deltaTime);
	// 剣を右手の骨に合わせます。アニメーションを進めた後に呼びます。
	void UpdateSword();
};

// 効果音です。シーンが持つ音源の組から鳴らします。音源がなければ何もしません(テスト中など)。
namespace SlowSound
{
	enum class Id
	{
		Hit,
		StrongHit,
		Bounce,
		Kill,
		Damage,
		TimeOn,
		Shot,
		Deflect,
		Count
	};
	void Play(Id id);
}

// 同じ音を重ねて鳴らせるよう、音ごとに複数の声を持つ音源の組です。シーンに1つだけ置きます。
class SlowSoundBank final : public GameObject
{
public:
	void Init() override;
	void Uninit() override;
	void Play(SlowSound::Id id);

private:
	static constexpr int kVoices = 3;
	Audio* m_Voices[static_cast<int>(SlowSound::Id::Count)][kVoices]{};
	int m_Next[static_cast<int>(SlowSound::Id::Count)]{};
};

// スコアとコンボです。シーンの初期化でResetし、最高記録だけはやり直しても残します。
class SlowScore
{
public:
	enum class Event
	{
		NormalHit,
		StrongHit,
		Pursuit,
		WallBounce,
		Deflect,
		JustSlow,
		KillMelee,
		KillGunner,
		KillBrute,
		Clear
	};

	struct Popup
	{
		std::wstring Text;
		// 表示してからの実時間です。
		float Age = 0.0f;
	};

	static void Reset();
	// ワールド時間でコンボの残り時間を減らし、実時間で表示の寿命を進めます。
	static void Update(float realDeltaTime, float worldDeltaTime);
	static void Add(Event event);
	// 被弾したらコンボを切ります。
	static void BreakCombo();

	static int GetScore();
	static int GetBestScore();
	static int GetCombo();
	static int GetBestCombo();
	static float GetMultiplier();
	// コンボが切れるまでの残り(0〜1)です。
	static float GetComboRemaining();
	// コンボ数から決まる評価です。コンボがなければ空文字です。
	static const wchar_t* GetRank();
	static const std::vector<Popup>& GetPopups();
};

class SlowEnemy final : public GameObject
{
public:
	// シーンファイルにはintで書きます(0=近接、1=射撃、2=重量)。
	enum class Kind
	{
		Melee,
		Gunner,
		Brute,
		Count
	};

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

	// 攻撃の重さです。Finisherは通常攻撃3段目で、真上へ打ち上げます(重量型の装甲は削りません)。
	enum class HitStrength
	{
		Normal,
		Finisher,
		Strong
	};

	// directionは水平の攻撃方向です。吹き飛ばす場合はこの向きへ飛びます。
	HitResult ApplyHit(const Vector3& direction, HitStrength strength, int damage);

	Kind GetKind() const { return m_Kind; }
	void SetKind(Kind kind) { m_KindValue = static_cast<int>(kind); OnPropertiesChanged(); }
	float GetRadius() const;
	State GetState() const { return m_State; }
	int GetHp() const { return m_Hp; }
	// 重量型の装甲の残りです。ほかの型は0です。
	int GetArmor() const { return m_Armor; }
	bool IsLaunched() const { return m_State == State::Launched; }
	// 着地までの残り時間(ワールド時間)です。吹き飛び中でなければ0です。
	float GetLaunchRemaining() const;
	int GetWallBounceCount() const { return m_WallBounces; }
	bool IsHanging() const { return m_State == State::Launched && m_HangTime > 0.0f; }
	// 殴りかかる予兆の残りがwindowSeconds以下なら、ジャスト減速の対象として1度だけtrueを返します(射撃型は弾で判定します)。
	bool TryJustSlow(float windowSeconds);

	// trueの間、敵のHPが減りません(吹き飛ばしと追撃の練習用)。
	static void SetInvincible(bool enabled) { s_Invincible = enabled; }
	static bool IsInvincible() { return s_Invincible; }

	// 近接型は人型のモデル(Enemy.fbx)で描き、歩き・被弾のモーションを再生します。ほかの型は今まで通りの形です。
	bool HasSkinnedBody() const { return m_Body != nullptr && m_Kind == Kind::Melee; }
	// 人型のモデルのベース層のクリップ名(Walk/Stand)と、重ねている被弾モーションの名前(Flinch/Knockdown、無ければ空)です。テスト用です。
	const std::string& GetBodyAnimationName() const;
	bool IsHitReactionPlaying() const;
	const std::string& GetHitReactionName() const;
	// 腰の骨が敵の位置から水平にどれだけ離れているか(m)。モーションで体が位置からずれていないかを確かめます。
	float GetBodyHipsOffset() const;

private:
	static bool s_Invincible;

	// 状態ごとの色違いのモデルです。型ごとに1組ずつ持ち、今の型と状態の1つだけを見せます。
	enum ModelSlot { Normal, Windup, Launched, Down, Flash, SlotCount };

	Collider* m_Collider = nullptr;
	ModelRenderer* m_Models[static_cast<int>(Kind::Count)][SlotCount]{};
	AnimationModel* m_Body = nullptr;
	// 歩きの再生速度を、実際に動いた距離から決めるための前フレームの位置です。
	Vector3 m_BodyPreviousPosition{ 0.0f, 0.0f, 0.0f };
	// 再生中のKnockdownの区間(クリップ上の秒)です。
	float m_KnockdownStartSeconds = 0.0f;
	float m_KnockdownEndSeconds = 0.0f;
	Kind m_Kind = Kind::Melee;
	int m_KindValue = 0;
	State m_State = State::Idle;
	float m_StateTime = 0.0f;
	int m_Hp = 0;
	int m_Armor = 0;
	// 被弾の白い光の残り時間(実時間)です。
	float m_FlashTime = 0.0f;
	// 1以上なら、この個体だけ調整値の代わりにこのHPで始めます。
	int m_HpOverride = 0;
	float m_LaunchScale = 1.0f;
	Vector3 m_LaunchDirection{ 0.0f, 0.0f, 1.0f };
	float m_LaunchTime = 0.0f;
	// 今の吹き飛びの距離・全体時間です(強攻撃と3段目で違います)。
	float m_LaunchDistance = 0.0f;
	float m_LaunchSeconds = 1.0f;
	float m_LaunchGravity = 0.0f;
	float m_VerticalSpeed = 0.0f;
	float m_HangTime = 0.0f;
	int m_WallBounces = 0;
	// 今の予兆でジャスト減速を取られたか。予兆に入るたびに戻します。
	bool m_JustSlowUsed = false;
	Vector3 m_StaggerVelocity{ 0.0f, 0.0f, 0.0f };
	// 射撃型が予兆に入った時点で決めた狙いの向きです。撃つまで変えないので、横へ動けば避けられます。
	Vector3 m_AimDirection{ 0.0f, 0.0f, 1.0f };

	void SetState(State state, float seconds = 0.0f);
	void Launch(const Vector3& direction, bool light = false);
	void UpdateLaunched(float worldDeltaTime);
	void UpdateChase(float worldDeltaTime);
	void UpdateGunnerChase(float worldDeltaTime);
	void MoveWithSeparation(Vector3 step);
	void Strike();
	void Fire();
	int GetMaxHp() const;
	void FaceTowards(const Vector3& direction);
	bool TryMove(const Vector3& offset);
	void RefreshModel();
	// 人型のモデルのクリップを状態と移動の速さから選び、ワールド時間で進めます。
	void UpdateBody(float worldDeltaTime);
	// 軽い被弾: 立ったままのけぞります。
	void PlayFlinch();
	// 吹き飛び: Knockdownをclip上のstartSecondsから再生し、secondsUntilLanding後に体が地面へ着くよう速さを合わせます。
	// 着地後は受け身の時間だけ倒れたままでいて、最後に立ち姿勢へ戻ります。
	void PlayKnockdown(float startSeconds, float secondsUntilLanding);
	// 再生中のKnockdownがクリップ上の何秒目か。再生していなければ0です。
	float GetKnockdownSeconds() const;
};

// 倒された近接型の敵の亡骸です。倒れるモーションを再生し、少し置いてから床へ沈んで消えます。
// 当たり判定もAIも持たないので、敵の数や狙いには含まれません。ステージの記述にも書かれません。
class SlowCorpse final : public GameObject
{
public:
	void Init() override;
	void Update() override;
	// startSecondsは倒れるモーションの開始位置(秒)です。吹き飛び中に倒した敵は、その続きから倒れます。
	void Begin(const Vector3& position, float yaw, float startSeconds = 0.0f);
	const std::string& GetAnimationName() const;

private:
	AnimationModel* m_Body = nullptr;
	float m_Age = 0.0f;
	float m_DyingSeconds = 0.0f;
	float m_FallSpeed = 0.0f;
};

// 射撃型の敵が撃つ弾です。ワールド時間で進むので、時間減速中は避けたり打ち返したりしやすくなります。
// プレイヤーの攻撃範囲に入ると打ち返され、今度は敵に当たって吹き飛ばします。
class SlowBullet final : public GameObject
{
public:
	void Init() override;
	void Update() override;

	void Launch(const Vector3& origin, const Vector3& velocity, std::uint64_t shooterId);
	// 打ち返します。撃った敵が残っていればそちらへ、いなければdirectionへ飛ばします。
	void Deflect(const Vector3& direction);
	bool IsDeflected() const { return m_Deflected; }
	// プレイヤーへ向かう弾がrange以内なら、ジャスト減速の対象として1度だけtrueを返します。
	bool TryJustSlow(const Vector3& playerPosition, float range);
	const Vector3& GetVelocity() const { return m_Velocity; }

private:
	ModelRenderer* m_Model = nullptr;
	ModelRenderer* m_DeflectedModel = nullptr;
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	std::uint64_t m_ShooterId = 0;
	float m_Life = 0.0f;
	bool m_Deflected = false;
	bool m_JustSlowUsed = false;
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
	DynamicText* m_ScoreText = nullptr;
	DynamicText* m_ComboText = nullptr;
	DynamicText* m_PopupText = nullptr;
	float m_RespawnTimer = 0.0f;
	bool m_WasSlow = false;
	// 全滅ボーナスを1回だけ出すためのフラグです。
	bool m_ClearAwarded = false;

	void UpdateHud();
	void RespawnEnemies();
	// 減速を発動した瞬間に、当たる直前の攻撃があればジャスト減速として報います。
	void CheckJustSlow();
};
