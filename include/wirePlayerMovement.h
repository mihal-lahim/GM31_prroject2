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
};
