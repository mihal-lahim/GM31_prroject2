#pragma once

namespace Engine
{
class GameTime
{
private:
	static float m_DeltaTime;
	static float m_ElapsedTime;
	static float m_TimeScale;
	static int m_HitStopFrames;
	static float m_HitStopScale;
	static bool m_Paused;
	static bool m_StepRequested;
	static bool m_SteppingThisFrame;
	static unsigned long long m_FrameCount;

public:
	static void Init();
	static void Update(float unscaledDeltaTime);

	static float GetDeltaTime() { return m_DeltaTime; }
	static float GetElapsedTime() { return m_ElapsedTime; }
	static unsigned long long GetFrameCount() { return m_FrameCount; }

	static void SetTimeScale(float timeScale);
	static float GetTimeScale() { return m_TimeScale; }
	static void StartHitStop(int frames, float timeScale);

	static void SetPaused(bool paused) { m_Paused = paused; m_StepRequested = false; }
	static bool IsPaused() { return m_Paused; }

	// コマ送り。ポーズ中に呼ぶと、次のUpdateで1フレームだけ通常どおり時間が進みます。
	static void RequestStepFrame() { if (m_Paused) { m_StepRequested = true; } }
	// Updateが要求を消化したフレームだけtrueです。ポーズ中でも更新を通す判定に使います。
	static bool IsSteppingFrame() { return m_SteppingThisFrame; }
};
}

// 既存ゲームコード向けの互換名です。新規コードはEngine::GameTimeを使用します。
using GameTime = Engine::GameTime;
