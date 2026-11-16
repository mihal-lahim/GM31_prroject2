#include "inputActions.h"
#include "keyboard_utf8.h"
#include "gameDefinition.h"
#include "classroomWalk.h"

bool RunClassroomRenderTest();
bool RunAnimatorSelfTests();

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<ClassroomWalkScene>();
	}
}

// 操作名と既定のキー・ボタンです。assets/config/classroom-input.cfg で差し替えられます。
static void DefineClassroomInputs()
{
	using Engine::InputActions;
	using Engine::InputBinding;
	namespace Pad = Engine::PadButtons;
	InputActions::Define("move_forward", { InputBinding::Key(KK_W), InputBinding::Key(KK_UP), InputBinding::PadAxis(Engine::PadLeftY, 1) });
	InputActions::Define("move_back", { InputBinding::Key(KK_S), InputBinding::Key(KK_DOWN), InputBinding::PadAxis(Engine::PadLeftY, -1) });
	InputActions::Define("move_right", { InputBinding::Key(KK_D), InputBinding::Key(KK_RIGHT), InputBinding::PadAxis(Engine::PadLeftX, 1) });
	InputActions::Define("move_left", { InputBinding::Key(KK_A), InputBinding::Key(KK_LEFT), InputBinding::PadAxis(Engine::PadLeftX, -1) });
	InputActions::DefineLookStick();
	InputActions::Define("jump", { InputBinding::Key(KK_SPACE), InputBinding::PadButton(Pad::A) });
	InputActions::Define("dance", { InputBinding::Key(KK_R), InputBinding::PadButton(Pad::Y) });
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition = []
	{
		GameDefinition value{
			"Classroom Walk",
			"assets/config/classroom.cfg",
			nullptr,
			CreateInitialScene,
			RunAnimatorSelfTests,
			RunClassroomRenderTest
		};
		value.DefineInputs = DefineClassroomInputs;
		value.InputBindingsPath = "assets/config/classroom-input.cfg";
		return value;
	}();
	return definition;
}
