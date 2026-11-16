#include "inputActions.h"

#include "keyboard_utf8.h"
#include "mouseInput.h"

#include <Windows.h>
#include <Xinput.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_map>

#pragma comment(lib, "xinput.lib")

namespace Engine
{
namespace
{
	struct ActionState
	{
		std::vector<InputBinding> Bindings;
		float Value = 0.0f;
		bool Down = false;
		bool Previous = false;
	};

	std::unordered_map<std::string, ActionState> g_Actions;

	// スティックの遊びです。これより小さい傾きは0として扱います。
	constexpr float kStickDeadZone = 0.24f;
	// ボタンとして見るときに「押している」とみなす量です。
	constexpr float kPressThreshold = 0.5f;

	XINPUT_STATE g_Pad{};
	bool g_PadConnected = false;

	float NormalizeStick(SHORT raw)
	{
		const float value = (std::max)(-1.0f, static_cast<float>(raw) / 32767.0f);
		const float magnitude = std::fabs(value);
		if (magnitude < kStickDeadZone)
		{
			return 0.0f;
		}
		return std::copysign((magnitude - kStickDeadZone) / (1.0f - kStickDeadZone), value);
	}

	float ReadPadAxis(int axis)
	{
		if (!g_PadConnected)
		{
			return 0.0f;
		}
		const XINPUT_GAMEPAD& pad = g_Pad.Gamepad;
		switch (axis)
		{
		case PadLeftX: return NormalizeStick(pad.sThumbLX);
		case PadLeftY: return NormalizeStick(pad.sThumbLY);
		case PadRightX: return NormalizeStick(pad.sThumbRX);
		case PadRightY: return NormalizeStick(pad.sThumbRY);
		case PadLeftTrigger: return pad.bLeftTrigger / 255.0f;
		case PadRightTrigger: return pad.bRightTrigger / 255.0f;
		default: return 0.0f;
		}
	}

	float ReadBinding(const InputBinding& binding)
	{
		switch (binding.Type)
		{
		case InputBinding::Kind::Key:
			return Keyboard_IsKeyDown(static_cast<Keyboard_Keys>(binding.Value)) ? 1.0f : 0.0f;
		case InputBinding::Kind::MouseLeft:
			return Mouse_IsLeftButtonDown() ? 1.0f : 0.0f;
		case InputBinding::Kind::MouseRight:
			return Mouse_IsRightButtonDown() ? 1.0f : 0.0f;
		case InputBinding::Kind::PadButton:
			return g_PadConnected && (g_Pad.Gamepad.wButtons & binding.Value) != 0 ? 1.0f : 0.0f;
		case InputBinding::Kind::PadAxis:
			return (std::max)(0.0f, ReadPadAxis(binding.Value) * static_cast<float>(binding.Sign));
		}
		return 0.0f;
	}

	std::string Trim(const std::string& text)
	{
		const auto first = text.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
		{
			return {};
		}
		const auto last = text.find_last_not_of(" \t\r\n");
		return text.substr(first, last - first + 1);
	}

	std::string ToUpper(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
		return text;
	}

	const std::unordered_map<std::string, InputBinding>& NamedBindings()
	{
		static const std::unordered_map<std::string, InputBinding> names = []
		{
			std::unordered_map<std::string, InputBinding> table;
			for (char c = 'A'; c <= 'Z'; ++c) table[std::string(1, c)] = InputBinding::Key(c);
			for (char c = '0'; c <= '9'; ++c) table[std::string(1, c)] = InputBinding::Key(c);
			for (int i = 1; i <= 12; ++i) table["F" + std::to_string(i)] = InputBinding::Key(VK_F1 + i - 1);
			table["SPACE"] = InputBinding::Key(VK_SPACE);
			table["ENTER"] = InputBinding::Key(VK_RETURN);
			table["ESCAPE"] = InputBinding::Key(VK_ESCAPE);
			table["ESC"] = InputBinding::Key(VK_ESCAPE);
			table["TAB"] = InputBinding::Key(VK_TAB);
			table["BACKSPACE"] = InputBinding::Key(VK_BACK);
			table["LSHIFT"] = InputBinding::Key(VK_LSHIFT);
			table["RSHIFT"] = InputBinding::Key(VK_RSHIFT);
			table["LCTRL"] = InputBinding::Key(VK_LCONTROL);
			table["RCTRL"] = InputBinding::Key(VK_RCONTROL);
			table["LALT"] = InputBinding::Key(VK_LMENU);
			table["UP"] = InputBinding::Key(VK_UP);
			table["DOWN"] = InputBinding::Key(VK_DOWN);
			table["LEFT"] = InputBinding::Key(VK_LEFT);
			table["RIGHT"] = InputBinding::Key(VK_RIGHT);
			table["MOUSE_LEFT"] = InputBinding::MouseLeft();
			table["MOUSE_RIGHT"] = InputBinding::MouseRight();
			table["PAD_A"] = InputBinding::PadButton(XINPUT_GAMEPAD_A);
			table["PAD_B"] = InputBinding::PadButton(XINPUT_GAMEPAD_B);
			table["PAD_X"] = InputBinding::PadButton(XINPUT_GAMEPAD_X);
			table["PAD_Y"] = InputBinding::PadButton(XINPUT_GAMEPAD_Y);
			table["PAD_LB"] = InputBinding::PadButton(XINPUT_GAMEPAD_LEFT_SHOULDER);
			table["PAD_RB"] = InputBinding::PadButton(XINPUT_GAMEPAD_RIGHT_SHOULDER);
			table["PAD_START"] = InputBinding::PadButton(XINPUT_GAMEPAD_START);
			table["PAD_BACK"] = InputBinding::PadButton(XINPUT_GAMEPAD_BACK);
			table["PAD_UP"] = InputBinding::PadButton(XINPUT_GAMEPAD_DPAD_UP);
			table["PAD_DOWN"] = InputBinding::PadButton(XINPUT_GAMEPAD_DPAD_DOWN);
			table["PAD_LEFT"] = InputBinding::PadButton(XINPUT_GAMEPAD_DPAD_LEFT);
			table["PAD_RIGHT"] = InputBinding::PadButton(XINPUT_GAMEPAD_DPAD_RIGHT);
			table["PAD_LS"] = InputBinding::PadButton(XINPUT_GAMEPAD_LEFT_THUMB);
			table["PAD_RS"] = InputBinding::PadButton(XINPUT_GAMEPAD_RIGHT_THUMB);
			table["PAD_LT"] = InputBinding::PadAxis(PadLeftTrigger, 1);
			table["PAD_RT"] = InputBinding::PadAxis(PadRightTrigger, 1);
			table["PAD_LX+"] = InputBinding::PadAxis(PadLeftX, 1);
			table["PAD_LX-"] = InputBinding::PadAxis(PadLeftX, -1);
			table["PAD_LY+"] = InputBinding::PadAxis(PadLeftY, 1);
			table["PAD_LY-"] = InputBinding::PadAxis(PadLeftY, -1);
			table["PAD_RX+"] = InputBinding::PadAxis(PadRightX, 1);
			table["PAD_RX-"] = InputBinding::PadAxis(PadRightX, -1);
			table["PAD_RY+"] = InputBinding::PadAxis(PadRightY, 1);
			table["PAD_RY-"] = InputBinding::PadAxis(PadRightY, -1);
			return table;
		}();
		return names;
	}

	const ActionState* Find(const std::string& action)
	{
		const auto found = g_Actions.find(action);
		return found != g_Actions.end() ? &found->second : nullptr;
	}
}

bool InputBinding::Parse(const std::string& name, InputBinding& binding)
{
	const auto& names = NamedBindings();
	const auto found = names.find(ToUpper(Trim(name)));
	if (found == names.end())
	{
		return false;
	}
	binding = found->second;
	return true;
}

void InputActions::Clear()
{
	g_Actions.clear();
}

void InputActions::Define(const std::string& action, std::initializer_list<InputBinding> bindings)
{
	ActionState& state = g_Actions[action];
	state.Bindings.assign(bindings.begin(), bindings.end());
}

bool InputActions::LoadBindings(const char* path, std::vector<std::string>* unknownNames)
{
	std::ifstream file(path);
	if (!file)
	{
		return false;
	}

	std::string line;
	while (std::getline(file, line))
	{
		const auto comment = line.find('#');
		if (comment != std::string::npos)
		{
			line.erase(comment);
		}
		const auto equals = line.find('=');
		if (equals == std::string::npos)
		{
			continue;
		}
		const std::string action = Trim(line.substr(0, equals));
		if (action.empty())
		{
			continue;
		}
		if (g_Actions.find(action) == g_Actions.end())
		{
			// ゲームが知らない操作名は、書き間違いとして知らせます。
			if (unknownNames != nullptr) unknownNames->push_back(action);
			continue;
		}

		std::vector<InputBinding> bindings;
		std::stringstream list(line.substr(equals + 1));
		std::string name;
		while (std::getline(list, name, ','))
		{
			if (Trim(name).empty())
			{
				continue;
			}
			InputBinding binding;
			if (InputBinding::Parse(name, binding))
			{
				bindings.push_back(binding);
			}
			else if (unknownNames != nullptr)
			{
				unknownNames->push_back(action + "=" + Trim(name));
			}
		}
		g_Actions[action].Bindings = std::move(bindings);
	}
	return true;
}

void InputActions::Update()
{
	g_PadConnected = XInputGetState(0, &g_Pad) == ERROR_SUCCESS;
	for (auto& [name, state] : g_Actions)
	{
		float value = 0.0f;
		for (const InputBinding& binding : state.Bindings)
		{
			value = (std::max)(value, ReadBinding(binding));
		}
		state.Previous = state.Down;
		state.Value = value;
		state.Down = value >= kPressThreshold;
	}
}

bool InputActions::IsDown(const std::string& action)
{
	const ActionState* state = Find(action);
	return state != nullptr && state->Down;
}

bool InputActions::IsTriggered(const std::string& action)
{
	const ActionState* state = Find(action);
	return state != nullptr && state->Down && !state->Previous;
}

bool InputActions::IsReleased(const std::string& action)
{
	const ActionState* state = Find(action);
	return state != nullptr && !state->Down && state->Previous;
}

float InputActions::GetValue(const std::string& action)
{
	const ActionState* state = Find(action);
	return state != nullptr ? state->Value : 0.0f;
}

float InputActions::GetAxis(const std::string& negative, const std::string& positive)
{
	return std::clamp(GetValue(positive) - GetValue(negative), -1.0f, 1.0f);
}

const std::vector<InputBinding>* InputActions::GetBindings(const std::string& action)
{
	const ActionState* state = Find(action);
	return state != nullptr ? &state->Bindings : nullptr;
}

bool InputActions::IsDefined(const std::string& action)
{
	return Find(action) != nullptr;
}
}
