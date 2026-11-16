#include "inputActions.h"
#include "keyboard_utf8.h"
#include "gameDefinition.h"
#include "fpsGame.h"

bool RunFpsRenderTest();

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<FpsGameScene>();
	}
}

// 操作名と既定のキー・ボタンです。assets/config/fps-input.cfg で差し替えられます。
static void DefineFpsInputs()
{
	using Engine::InputActions;
	using Engine::InputBinding;
	namespace Pad = Engine::PadButtons;
	InputActions::Define("move_forward", { InputBinding::Key(KK_W), InputBinding::PadAxis(Engine::PadLeftY, 1) });
	InputActions::Define("move_back", { InputBinding::Key(KK_S), InputBinding::PadAxis(Engine::PadLeftY, -1) });
	InputActions::Define("move_right", { InputBinding::Key(KK_D), InputBinding::PadAxis(Engine::PadLeftX, 1) });
	InputActions::Define("move_left", { InputBinding::Key(KK_A), InputBinding::PadAxis(Engine::PadLeftX, -1) });
	InputActions::DefineLookStick();
	InputActions::Define("sprint", { InputBinding::Key(KK_LEFTSHIFT), InputBinding::PadButton(Pad::LeftThumb) });
	InputActions::Define("jump", { InputBinding::Key(KK_SPACE), InputBinding::PadButton(Pad::A) });
	InputActions::Define("fire", { InputBinding::MouseLeft(), InputBinding::PadAxis(Engine::PadRightTrigger, 1) });
	InputActions::Define("aim", { InputBinding::MouseRight(), InputBinding::PadAxis(Engine::PadLeftTrigger, 1) });
	InputActions::Define("retry", { InputBinding::Key(KK_R), InputBinding::PadButton(Pad::Back) });
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition = []
	{
		GameDefinition value{
			"FPS Game",
			"assets/config/fps.cfg",
			nullptr,
			CreateInitialScene,
			nullptr,
			RunFpsRenderTest
		};
		value.DefineInputs = DefineFpsInputs;
		value.InputBindingsPath = "assets/config/fps-input.cfg";
		return value;
	}();
	return definition;
}
