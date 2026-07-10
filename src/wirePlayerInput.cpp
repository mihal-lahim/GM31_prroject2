#include "wirePlayerInput.h"

#include "keyboard_utf8.h"
#include "main.h"

WirePlayerInputState WirePlayerInput::Capture()
{
	WirePlayerInputState input;
	input.MoveForward = (Keyboard_IsKeyDown(KK_W) ? 1.0f : 0.0f)
		- (Keyboard_IsKeyDown(KK_S) ? 1.0f : 0.0f);
	input.MoveRight = (Keyboard_IsKeyDown(KK_D) ? 1.0f : 0.0f)
		- (Keyboard_IsKeyDown(KK_A) ? 1.0f : 0.0f);
	input.JumpPressed = Keyboard_IsKeyDownTrigger(KK_SPACE);
	input.BlinkDown = Keyboard_IsKeyDown(KK_LEFTSHIFT) || Keyboard_IsKeyDown(KK_RIGHTSHIFT);
	input.AimDown = Mouse_IsRightButtonDown();
	input.AimPressed = Mouse_IsRightButtonDownTrigger();
	input.AttackDown = Mouse_IsLeftButtonDown();
	input.AttackPressed = Mouse_IsLeftButtonDownTrigger();
	return input;
}
