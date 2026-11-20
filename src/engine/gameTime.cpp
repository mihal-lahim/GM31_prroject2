#include "gameTime.h"

#include <algorithm>
#include <cmath>

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
int GameTime::m_HitStopTotalFrames = 0;
bool GameTime::m_HitStopThisFrame = false;
float GameTime::m_DeltaTimeIgnoringHitStop = kDefaultDeltaTime;
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
	m_HitStopTotalFrames = 0;
	m_HitStopThisFrame = false;
	m_DeltaTimeIgnoringHitStop = kDefaultDeltaTime;
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
		m_DeltaTimeIgnoringHitStop = kStoppedDeltaTime;
		// このフレームはヒットストップで止めていません。直前に最後の1フレームを消化していても、
		// ポーズ中に「ヒットストップ中」が残り続けないようにします(残りフレームはポーズ解除後に再開します)。
		m_HitStopThisFrame = false;
	}
	else
	{
		// NaN・無限大の経過時間は0として扱います。一度でも入ると経過時間の合計が戻らなくなるためです。
		m_DeltaTime = std::isfinite(unscaledDeltaTime) ? std::max(0.0f, unscaledDeltaTime) : 0.0f;
		m_DeltaTime *= m_TimeScale;
		// 有限どうしの掛け算でも、大きすぎると無限大になります。
		if (!std::isfinite(m_DeltaTime))
		{
			m_DeltaTime = 0.0f;
		}
		m_DeltaTimeIgnoringHitStop = m_DeltaTime;
		m_HitStopThisFrame = m_HitStopFrames > 0;
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
	// NaN・無限大は無視し、直前の倍率を保ちます。
	if (!std::isfinite(timeScale))
	{
		return;
	}
	if (timeScale < kMinimumTimeScale)
	{
		timeScale = kMinimumTimeScale;
	}

	m_TimeScale = timeScale;
}

void GameTime::StartHitStop(int frames, float timeScale)
{
	if (frames <= 0 || !std::isfinite(timeScale))
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
		m_HitStopTotalFrames = frames;
	}
}
}

namespace Engine
{
void GameTime::SetChannelScale(int channel, float scale)
{
	// チャンネル0は全体の時間そのものなので変えません。
	if (channel <= 0 || channel >= kMaxTimeChannels || channel == kHitStopFreeChannel
		|| !std::isfinite(scale))
	{
		return;
	}
	m_ChannelScales[channel] = std::max(kMinimumTimeScale, scale);
}

float GameTime::GetChannelScale(int channel)
{
	if (channel <= 0 || channel >= kMaxTimeChannels || channel == kHitStopFreeChannel)
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

namespace Engine
{
float GameTime::GetHitStopRemainingRatio()
{
	// StartHitStop を呼んだフレーム(まだ止めていない)では残り=全体なので1、止めているフレームは
	// そのフレームの分も含めて数え、最後に止めたフレームで 1/全体、終われば0になります。
	const int remaining = m_HitStopFrames + (m_HitStopThisFrame ? 1 : 0);
	if (m_HitStopTotalFrames <= 0 || remaining <= 0)
	{
		return 0.0f;
	}
	return std::min(1.0f, static_cast<float>(remaining) / static_cast<float>(m_HitStopTotalFrames));
}
}
