#pragma once

#include "Vector3.h"

class WirePlayer;
class WireTpsCamera;
struct WirePlayerInputState;

class WirePlayerMovement
{
private:
	bool m_OnGround = false;
	float m_MoveAnimation = 0.0f;
	Vector3 m_BlinkDirection{ 0.0f, 0.0f, 1.0f };
	float m_BlinkCooldown = 0.0f;
	float m_BlinkTimeRemaining = 0.0f;
	// このブリンクで維持する速さ。開始時に現在速度から決まります(勢い保存)。
	float m_BlinkTravelSpeed = 0.0f;
	// 空中で消費した回数。着地またはワイヤー接続で0に戻ります。
	int m_AirBlinkChargesUsed = 0;
	bool m_BlinkInputHeld = false;

	void UpdateBlink(WirePlayer& player, float dt, const WirePlayerInputState& input,
		const Vector3& cameraForward, const Vector3& cameraRight);
	Vector3 GetBlinkInputDirection(const WirePlayerInputState& input,
		const Vector3& cameraForward, const Vector3& cameraRight) const;
	void StartBlink(WirePlayer& player, const Vector3& direction);
	void EmitBlinkParticles(const Vector3& position, const Vector3& direction, int count);
	void EmitBlinkShockwave(const Vector3& position, const Vector3& direction, int count);

public:
	void Update(WirePlayer& player, float dt, const WirePlayerInputState& input, const WireTpsCamera* camera);
	void ResolvePlatformLanding(WirePlayer& player, const Vector3& previousPosition);
	void SetOnGround(bool onGround) { m_OnGround = onGround; }
	bool IsOnGround() const { return m_OnGround; }
	// ブリンク移動中はワイヤーの開始・牽引を抑止し、勢い保存が破られないようにします。
	bool IsBlinking() const { return m_BlinkTimeRemaining > 0.0f; }
	// リスポーン時にブリンクの持続・維持速度・チャージを初期化します。
	void ResetForRespawn();
	// ワイヤー接続の瞬間に呼ばれ、空中チャージを即時回復します(UIの表示遅れ防止)。
	void RestoreAirBlinkCharges() { m_AirBlinkChargesUsed = 0; }

	// UIゲージ用: クールダウンの回復率(0=撃った直後、1=完了)です。
	float GetBlinkCooldownRatio01() const;
	// クールダウン完了かつチャージがあり、いま入力すればブリンクできる状態です。
	bool CanBlinkNow() const;
	int GetAirBlinkChargesRemaining() const;
};
