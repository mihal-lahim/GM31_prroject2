#pragma once

#include "gameObject.h"
#include "wireBreakable.h"

class WirePlayer;

// 屋上を巡回し、プレイヤーを見つけると追跡して接触攻撃する敵です。
// 足場の縁で引き返すため、自分から落下することはありません。
class WireEnemy : public GameObject, public IWireBreakable
{
public:
	enum class State
	{
		Patrol,
		Chase,
		Attack
	};

private:
	static constexpr float kDefaultScale = 1.12f;
	static constexpr float kDefaultHitRadius = 1.1f;

	Vector3 m_BaseScale{ kDefaultScale, kDefaultScale, kDefaultScale };
	float m_HitRadius = kDefaultHitRadius;
	float m_HitReactionTime = 0.0f;
	float m_HitLean = 0.0f;
	int m_TotalDamage = 0;

	State m_State = State::Patrol;
	Vector3 m_Home{ 0.0f, 0.0f, 0.0f };
	Vector3 m_MoveDirection{ 0.0f, 0.0f, 1.0f };
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	float m_PatrolTurnTimer = 0.0f;
	float m_AttackCooldown = 0.0f;
	int m_Health = 1;

	// 巡回できる足場の範囲です。生成した足場を指すポインタではなく値で持つため、
	// 足場が破棄されても不正参照になりません。
	Vector3 m_AreaCenter{ 0.0f, 0.0f, 0.0f };
	float m_AreaHalfX = 0.0f;
	float m_AreaHalfZ = 0.0f;
	bool m_HasArea = false;

	void ResolvePatrolArea();
	void UpdateThinking(WirePlayer* player, float dt);
	void TurnPatrolDirection(float angle);
	bool IsInsideArea(const Vector3& position) const;
	void ApplyMovement(const Vector3& desiredDirection, float speed, float dt);
	void EmitDeathEffect() const;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;

	// 配置後に一度呼び、現在位置を巡回の中心と足場範囲として確定させます。
	void OnPlaced();

	Vector3 GetHitCenter() const;
	float GetHitRadius() const { return m_HitRadius; }
	void OnSwordHit(int damage, const Vector3& hitDirection);
	int GetTotalDamage() const { return m_TotalDamage; }

	State GetState() const { return m_State; }
	// デバッグ表示用。思考ルーチンが動いていることを画面で確認できます。
	const char* GetStateName() const;
	int GetHealth() const { return m_Health; }

	// 剣からは「破壊できる小型ロボット」として一律に扱われます。
	Vector3 GetBreakableCenter() const override { return GetHitCenter(); }
	float GetBreakableRadius() const override { return m_HitRadius; }
	bool IsBreakableAlive() const override { return !m_Destroy && m_Health > 0; }
	Kind GetBreakableKind() const override { return Kind::SmallRobot; }
	bool ApplyBreakDamage(int damage, const Vector3& hitDirection) override;
};
