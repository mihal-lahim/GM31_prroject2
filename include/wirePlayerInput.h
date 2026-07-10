#pragma once

struct WirePlayerInputState
{
	float MoveForward = 0.0f;
	float MoveRight = 0.0f;
	bool JumpPressed = false;
	bool BlinkDown = false;
	bool AimDown = false;
	bool AimPressed = false;
	bool AttackDown = false;
	bool AttackPressed = false;
};

class WirePlayerInput
{
public:
	static WirePlayerInputState Capture();
};
