#include "gameTime.h"

#include <algorithm>

namespace Engine
{
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
bool GameTime::m_StepRequested = false;
bool GameTime::m_SteppingThisFrame = false;
unsigned long long GameTime::m_FrameCount = 0;
float GameTime::m_ChannelScales[GameTime::kMaxTimeChannels] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

void GameTime::Init()
{
	m_DeltaTime = kDefaultDeltaTime;
	m_ElapsedTime = 0.0f;
	m_TimeScale = kDefaultTimeScale;
	m_HitStopFrames = 0;
	m_HitStopScale = kDefaultTimeScale;
	m_Paused = false;
	m_StepRequested = false;
	m_SteppingThisFrame = false;
	m_FrameCount = 0;
	ResetChannels();
}

void GameTime::Update(float unscaledDeltaTime)
{
	// コマ送り要求は、要求されたフレームではなく次のUpdateで一度だけ消化します。
	// 消化したフレームだけIsSteppingFrameがtrueになり、更新側が同じ判断を共有できます。
	m_SteppingThisFrame = m_Paused && m_StepRequested;
	m_StepRequested = false;

	if (m_Paused && !m_SteppingThisFrame)
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
}

namespace Engine
{
void GameTime::SetChannelScale(int channel, float scale)
{
	// チャンネル0は全体の時間そのものなので変えません。
	if (channel <= 0 || channel >= kMaxTimeChannels)
	{
		return;
	}
	m_ChannelScales[channel] = std::max(kMinimumTimeScale, scale);
}

float GameTime::GetChannelScale(int channel)
{
	if (channel <= 0 || channel >= kMaxTimeChannels)
	{
		return 1.0f;
	}
	return m_ChannelScales[channel];
}

void GameTime::ResetChannels()
{
	for (float& scale : m_ChannelScales)
	{
		scale = 1.0f;
	}
}
}
