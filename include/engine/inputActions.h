#pragma once

#include <initializer_list>
#include <string>
#include <vector>

// 操作名(「攻撃」「ダッシュ」など)とキー・ボタンの対応を持つ入力層です。
// ゲームは Keyboard_IsKeyDown(KK_W) の代わりに InputActions::IsDown("move_forward") のように読みます。
//   - 既定の割り当てはゲームが Define で決めます。
//   - 設定ファイル(例: assets/config/slow-input.cfg)の「操作名 = W, UP, PAD_A」で差し替えられます(キーコンフィグ)。
//   - XInput のゲームパッド(1台目)にも割り当てられます。
namespace Engine
{
struct InputBinding
{
	enum class Kind
	{
		Key,
		MouseLeft,
		MouseRight,
		PadButton,
		// スティックやトリガーを向きつきで使います。Valueは軸番号、Signは+1か-1です。
		PadAxis
	};

	Kind Type = Kind::Key;
	int Value = 0;
	int Sign = 1;

	static InputBinding Key(int virtualKey) { return { Kind::Key, virtualKey, 1 }; }
	static InputBinding MouseLeft() { return { Kind::MouseLeft, 0, 1 }; }
	static InputBinding MouseRight() { return { Kind::MouseRight, 0, 1 }; }
	// XINPUT_GAMEPAD_A などのビットです。
	static InputBinding PadButton(int mask) { return { Kind::PadButton, mask, 1 }; }
	static InputBinding PadAxis(int axis, int sign) { return { Kind::PadAxis, axis, sign }; }

	// "W" "SPACE" "MOUSE_LEFT" "PAD_A" "PAD_LX+" のような名前から作ります。知らない名前ならfalseです。
	static bool Parse(const std::string& name, InputBinding& binding);
};

// PadButton に渡すビットです(XINPUT_GAMEPAD_* と同じ値)。ゲーム側が Xinput.h を読まずに済むよう置いています。
namespace PadButtons
{
	inline constexpr int Up = 0x0001, Down = 0x0002, Left = 0x0004, Right = 0x0008;
	inline constexpr int Start = 0x0010, Back = 0x0020, LeftThumb = 0x0040, RightThumb = 0x0080;
	inline constexpr int LeftShoulder = 0x0100, RightShoulder = 0x0200;
	inline constexpr int A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000;
}

// PadAxis の軸番号です。
enum PadAxisId
{
	PadLeftX = 0,
	PadLeftY = 1,
	PadRightX = 2,
	PadRightY = 3,
	PadLeftTrigger = 4,
	PadRightTrigger = 5,
};

class InputActions
{
public:
	// 割り当てをすべて消します。ゲームの初期化の最初に呼びます。
	static void Clear();
	// 操作名の既定の割り当てを決めます。同じ名前で呼び直すと置き換えます。
	static void Define(const std::string& action, std::initializer_list<InputBinding> bindings);
	// 設定ファイルから割り当てを読み、書かれている操作だけ差し替えます。
	// ファイルが無ければ何もしません。読めなかった名前は unknownNames に入れます(nullptr可)。
	static bool LoadBindings(const char* path, std::vector<std::string>* unknownNames = nullptr);

	// 1ステップに1回、Managerが呼びます。押した瞬間・離した瞬間の判定はここで確定します。
	static void Update();

	static bool IsDown(const std::string& action);
	static bool IsTriggered(const std::string& action);
	static bool IsReleased(const std::string& action);
	// 0〜1の強さです。キーは0か1、スティックとトリガーは倒した量です。
	static float GetValue(const std::string& action);
	// positive - negative を -1〜1 で返します(移動入力向け)。
	static float GetAxis(const std::string& negative, const std::string& positive);

	// 右スティックの視点操作です。DefineLookStick で look_left/right/up/down を登録し、
	// GetLookDelta で今のステップに回す量(ラジアン)を得ます。上に倒すとpitchが増えます。
	static void DefineLookStick();
	static void GetLookDelta(float deltaTime, float& yaw, float& pitch);

	static const std::vector<InputBinding>* GetBindings(const std::string& action);
	static bool IsDefined(const std::string& action);
};
}
