#pragma once

#include "main.h"

class GameTime
{
private:
	static DWORD m_LastTime;
	static float m_DeltaTime;
	static float m_ElapsedTime;
	static float m_TimeScale;
	static bool m_Paused;
	static bool m_FirstFrame;
	static unsigned long long m_FrameCount;

public:
	static void Init();
	static void Update();

	static float GetDeltaTime() { return m_DeltaTime; }
	static float GetElapsedTime() { return m_ElapsedTime; }
	static unsigned long long GetFrameCount() { return m_FrameCount; }

	static void SetTimeScale(float timeScale);
	static float GetTimeScale() { return m_TimeScale; }

	static void SetPaused(bool paused) { m_Paused = paused; }
	static bool IsPaused() { return m_Paused; }
};
