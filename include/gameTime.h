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
	static float m_ChannelScales[];

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

	// 時間チャンネル: 全体の時間(GetDeltaTime)に、チャンネルごとの倍率を掛けた時間です。
	// 「敵だけ遅い」のように一部の時間だけを変えたいときに使います。チャンネル0は常に倍率1です。
	// ヒットストップとポーズは全体の時間に掛かるため、どのチャンネルにも同じく効きます。
	static constexpr int kMaxTimeChannels = 8;
	static void SetChannelScale(int channel, float scale);
	static float GetChannelScale(int channel);
	static float GetChannelDeltaTime(int channel) { return m_DeltaTime * GetChannelScale(channel); }
	static void ResetChannels();
};
}

// 既存ゲームコード向けの互換名です。新規コードはEngine::GameTimeを使用します。
using GameTime = Engine::GameTime;
