#pragma once

#include "gameObject.h"

class WirePlayer;

// 50mごとの踊り場を巡回し、プレイヤーを見つけると追跡して接触攻撃する敵です。
// 足場の縁で引き返すため、自分から落下することはありません。
class WireEnemy : public GameObject
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

	State m_State = State::Patrol;
	Vector3 m_Home{ 0.0f, 0.0f, 0.0f };
	Vector3 m_MoveDirection{ 0.0f, 0.0f, 1.0f };
	float m_HoverTime = 0.0f;
	float m_ChaseTargetY = 0.0f;
	bool m_Chasing = false;
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	float m_PatrolTurnTimer = 0.0f;
	float m_AttackCooldown = 0.0f;

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

public:
	void Init() override;
	void Uninit() override;
	void Update() override;

	// 配置後に一度呼び、現在位置を巡回の中心と足場範囲として確定させます。
	void OnPlaced();

	Vector3 GetHitCenter() const;
	float GetHitRadius() const { return m_HitRadius; }

	State GetState() const { return m_State; }
	// デバッグ表示用。思考ルーチンが動いていることを画面で確認できます。
	const char* GetStateName() const;

};
