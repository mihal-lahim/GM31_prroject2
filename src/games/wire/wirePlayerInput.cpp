#include "wirePlayerInput.h"

#include "inputActions.h"
#include "keyboard_utf8.h"

using Engine::InputActions;
using Engine::InputBinding;

WirePlayerInputState WirePlayerInput::Capture()
{
	WirePlayerInputState input;
	input.MoveForward = InputActions::GetAxis("move_back", "move_forward");
	input.MoveRight = InputActions::GetAxis("move_left", "move_right");
	input.JumpPressed = InputActions::IsTriggered("jump");
	input.BlinkDown = InputActions::IsDown("blink");
	input.AimDown = InputActions::IsDown("aim");
	input.AimPressed = InputActions::IsTriggered("aim");
	input.AttackDown = InputActions::IsDown("attack");
	input.AttackPressed = InputActions::IsTriggered("attack");
	return input;
}

void WirePlayerInput::DefineDefaults()
{
	namespace Pad = Engine::PadButtons;
	InputActions::Define("move_forward", { InputBinding::Key(KK_W), InputBinding::PadAxis(Engine::PadLeftY, 1) });
	InputActions::Define("move_back", { InputBinding::Key(KK_S), InputBinding::PadAxis(Engine::PadLeftY, -1) });
	InputActions::Define("move_right", { InputBinding::Key(KK_D), InputBinding::PadAxis(Engine::PadLeftX, 1) });
	InputActions::Define("move_left", { InputBinding::Key(KK_A), InputBinding::PadAxis(Engine::PadLeftX, -1) });
	InputActions::Define("jump", { InputBinding::Key(KK_SPACE), InputBinding::PadButton(Pad::A) });
	InputActions::Define("blink", { InputBinding::Key(KK_LEFTSHIFT), InputBinding::Key(KK_RIGHTSHIFT), InputBinding::PadButton(Pad::B) });
	InputActions::Define("aim", { InputBinding::MouseRight(), InputBinding::PadAxis(Engine::PadLeftTrigger, 1) });
	InputActions::Define("attack", { InputBinding::MouseLeft(), InputBinding::PadAxis(Engine::PadRightTrigger, 1) });
	// 画面の操作です。
	InputActions::Define("confirm", { InputBinding::Key(KK_ENTER), InputBinding::PadButton(Pad::A) });
	InputActions::Define("back", { InputBinding::Key(KK_ESCAPE), InputBinding::PadButton(Pad::Back) });
	InputActions::Define("retry", { InputBinding::Key(KK_R), InputBinding::PadButton(Pad::Y) });
	InputActions::Define("pause", { InputBinding::Key(KK_TAB), InputBinding::PadButton(Pad::Start) });
	InputActions::Define("tutorial", { InputBinding::Key(KK_T), InputBinding::PadButton(Pad::X) });
}
