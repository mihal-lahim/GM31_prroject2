#pragma once

class GameTime
{
private:
	static float m_DeltaTime;
	static float m_ElapsedTime;
	static float m_TimeScale;
	static int m_HitStopFrames;
	static float m_HitStopScale;
	static bool m_Paused;
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

	static void SetPaused(bool paused) { m_Paused = paused; }
	static bool IsPaused() { return m_Paused; }
};
