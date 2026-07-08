#include "main.h"
#include "gameTime.h"

DWORD GameTime::m_LastTime = 0;
float GameTime::m_DeltaTime = 1.0f / 60.0f;
float GameTime::m_ElapsedTime = 0.0f;
float GameTime::m_TimeScale = 1.0f;
bool GameTime::m_Paused = false;
bool GameTime::m_FirstFrame = true;
unsigned long long GameTime::m_FrameCount = 0;

void GameTime::Init()
{
	m_LastTime = timeGetTime();
	m_DeltaTime = 1.0f / 60.0f;
	m_ElapsedTime = 0.0f;
	m_TimeScale = 1.0f;
	m_Paused = false;
	m_FirstFrame = true;
	m_FrameCount = 0;
}

void GameTime::Update()
{
	DWORD currentTime = timeGetTime();
	const float fixedDeltaTime = 1.0f / 60.0f;

	if (m_Paused)
	{
		m_DeltaTime = 0.0f;
	}
	else
	{
		// Use a fixed timestep so gameplay feels frame-consistent.
		m_DeltaTime = fixedDeltaTime;
		m_DeltaTime *= m_TimeScale;
		m_ElapsedTime += m_DeltaTime;
	}

	m_LastTime = currentTime;
	m_FirstFrame = false;
	m_FrameCount++;
}

void GameTime::SetTimeScale(float timeScale)
{
	if (timeScale < 0.0f)
	{
		timeScale = 0.0f;
	}

	m_TimeScale = timeScale;
}
