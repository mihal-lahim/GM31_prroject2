#include "gameTime.h"

#include <algorithm>

namespace
{
	constexpr float kDefaultDeltaTime = 1.0f / 60.0f;
	constexpr float kDefaultTimeScale = 1.0f;
	constexpr float kStoppedDeltaTime = 0.0f;
	constexpr float kMinimumTimeScale = 0.0f;
	constexpr float kMaximumHitStopScale = 1.0f;
}

float GameTime::m_DeltaTime = kDefaultDeltaTime;
float GameTime::m_ElapsedTime = 0.0f;
float GameTime::m_TimeScale = kDefaultTimeScale;
int GameTime::m_HitStopFrames = 0;
float GameTime::m_HitStopScale = kDefaultTimeScale;
bool GameTime::m_Paused = false;
unsigned long long GameTime::m_FrameCount = 0;

void GameTime::Init()
{
	m_DeltaTime = kDefaultDeltaTime;
	m_ElapsedTime = 0.0f;
	m_TimeScale = kDefaultTimeScale;
	m_HitStopFrames = 0;
	m_HitStopScale = kDefaultTimeScale;
	m_Paused = false;
	m_FrameCount = 0;
}

void GameTime::Update(float unscaledDeltaTime)
{
	if (m_Paused)
	{
		m_DeltaTime = kStoppedDeltaTime;
	}
	else
	{
		m_DeltaTime = std::max(0.0f, unscaledDeltaTime);
		m_DeltaTime *= m_TimeScale;
		if (m_HitStopFrames > 0)
		{
			m_DeltaTime *= m_HitStopScale;
			m_HitStopFrames--;
		}
		m_ElapsedTime += m_DeltaTime;
	}

	m_FrameCount++;
}

void GameTime::SetTimeScale(float timeScale)
{
	if (timeScale < kMinimumTimeScale)
	{
		timeScale = kMinimumTimeScale;
	}

	m_TimeScale = timeScale;
}

void GameTime::StartHitStop(int frames, float timeScale)
{
	if (frames <= 0)
	{
		return;
	}

	if (timeScale < kMinimumTimeScale)
	{
		timeScale = kMinimumTimeScale;
	}
	if (timeScale > kMaximumHitStopScale)
	{
		timeScale = kMaximumHitStopScale;
	}

	if (frames >= m_HitStopFrames)
	{
		m_HitStopFrames = frames;
		m_HitStopScale = timeScale;
	}
}
