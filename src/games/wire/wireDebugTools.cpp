#include "wireDebugTools.h"

#include "collider.h"
#include "gameTime.h"
#include "keyboard_utf8.h"

#include <iterator>

namespace
{
	// 通常プレイを一切妨げないよう、すべて既定でOFFから始まります。
	bool s_OverlayVisible = false;
	bool s_Invincible = false;
	int s_TimeScaleIndex = 1;

	// F5で巡回するタイムスケールです。スローはワイヤーの挙動確認に使います。
	constexpr float kTimeScales[] = { 0.25f, 1.0f, 2.0f };
	constexpr int kTimeScaleCount = static_cast<int>(std::size(kTimeScales));
}

namespace WireDebugTools
{
	SceneRequest Update()
	{
		if (Keyboard_IsKeyDownTrigger(KK_F1))
		{
			s_OverlayVisible = !s_OverlayVisible;
		}
		if (Keyboard_IsKeyDownTrigger(KK_F2))
		{
			Collider::SetDebugDrawEnabled(!Collider::GetDebugDrawEnabled());
		}
		if (Keyboard_IsKeyDownTrigger(KK_F3))
		{
			// ポーズ中のみ有効です。GameTime側で無視されます。
			GameTime::RequestStepFrame();
		}
		if (Keyboard_IsKeyDownTrigger(KK_F4))
		{
			s_Invincible = !s_Invincible;
		}
		if (Keyboard_IsKeyDownTrigger(KK_F5))
		{
			s_TimeScaleIndex = (s_TimeScaleIndex + 1) % kTimeScaleCount;
			GameTime::SetTimeScale(kTimeScales[s_TimeScaleIndex]);
		}

		if (Keyboard_IsKeyDownTrigger(KK_F7)) return SceneRequest::Title;
		if (Keyboard_IsKeyDownTrigger(KK_F8)) return SceneRequest::Game;
		if (Keyboard_IsKeyDownTrigger(KK_F9)) return SceneRequest::ResultCleared;
		if (Keyboard_IsKeyDownTrigger(KK_F10)) return SceneRequest::ResultGameOver;
		return SceneRequest::None;
	}

	bool IsOverlayVisible() { return s_OverlayVisible; }
	bool IsInvincibleEnabled() { return s_Invincible; }
	float GetTimeScale() { return kTimeScales[s_TimeScaleIndex]; }

	void ResetForNewGame()
	{
		// 表示のON/OFFは操作した人の意図なので残し、ゲームプレイを変える設定だけ戻します。
		s_Invincible = false;
		s_TimeScaleIndex = 1;
		GameTime::SetTimeScale(kTimeScales[s_TimeScaleIndex]);
		Collider::SetDebugDrawEnabled(false);
	}

	const char* GetHelpLine1()
	{
		return "F1 OVERLAY  F2 COLLIDER  F3 STEP IN PAUSE  F4 INVINCIBLE";
	}

	const char* GetHelpLine2()
	{
		return "F5 TIMESCALE  F7 TITLE  F8 GAME  F9 CLEAR  F10 GAMEOVER";
	}
}
