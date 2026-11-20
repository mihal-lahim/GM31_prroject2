#include "slowAction.h"

#include "slowActionTuning.h"
#include "gameTime.h"
#include "particleSystem.h"
#include "renderer.h"

#include <random>
#include <string_view>

// 時間減速の画面演出です。①切り替わりの衝撃(衝撃波・フラッシュ)、②遅くなった世界(彩度低下・寒色・
// ビネットと、宙に止まる光の粒)を受け持ちます。③自分だけ速い対比(軌跡)は SlowPlayer が出します。

namespace
{
	constexpr std::string_view kPostEffectId = "post-realistic";
	// 光の粒の寿命(全体の時間でのフレーム数)と、撒く範囲(プレイヤーからの水平距離・高さ)です。
	constexpr int kDustLifeFrames = 300;
	constexpr float kDustRadius = 13.0f;
	constexpr float kDustMinHeight = 0.2f;
	constexpr float kDustMaxHeight = 4.5f;

	// 衝撃波の経過時間(実時間)です。負なら衝撃波は出ていません。
	float g_WaveTime = -1.0f;
	float g_WaveSign = 1.0f;
	float g_Flash = 0.0f;
	bool g_WasSlow = false;
	// ジャスト減速の発動と同じフレームは、フラッシュを減衰させずに最大値で1フレーム描きます。
	bool g_FlashFresh = false;
	// 光の粒を撒く量の端数です(1ステップに1粒未満のことが多いので持ち越します)。
	float g_DustCarry = 0.0f;
	// 始まってすぐ闘技場に粒が漂っているよう、最初の Update でまとめて撒きます。
	bool g_DustWarmed = false;
	// ポストエフェクトが読み込めなかったら、毎フレーム読み直さないようにします。
	bool g_PostUnavailable = false;
	// 時間減速の画面演出(彩度・寒色・衝撃波・フラッシュ)を、いま画面へ出しているかです。
	bool g_TimeSlowShown = false;
	// ほかの演出の火花と乱数を分け、既存の火花の出方を変えないようにします。
	std::mt19937 g_Random{ 20261120u };

	float Random(float minimum, float maximum)
	{
		return std::uniform_real_distribution<float>(minimum, maximum)(g_Random);
	}

	void EmitDust(const Vector3& focus, int lifeFrames)
	{
		const float limit = SlowActionScene::kArenaHalfSize - 0.5f;
		const float angle = Random(0.0f, XM_2PI);
		const float distance = kDustRadius * sqrtf(Random(0.0f, 1.0f));
		const Vector3 position{
			std::clamp(focus.x + cosf(angle) * distance, -limit, limit),
			Random(kDustMinHeight, kDustMaxHeight),
			std::clamp(focus.z + sinf(angle) * distance, -limit, limit) };
		const Vector3 drift{ Random(-0.25f, 0.25f), Random(0.03f, 0.2f), Random(-0.25f, 0.25f) };
		const float warmth = Random(0.0f, 1.0f);
		ParticleSystem::Emit(position, drift, Random(0.05f, 0.11f), lifeFrames,
			{ 0.7f + 0.2f * warmth, 0.85f, 1.0f, 0.35f }, 0.0f, 0.9995f, SlowClock::kWorldTimeChannel);
	}

	void UpdateDust(float realDeltaTime, const Vector3& focus)
	{
		const int count = GetSlowActionTuning().SlowFxDustCount;
		if (count <= 0)
		{
			return;
		}
		if (!g_DustWarmed)
		{
			g_DustWarmed = true;
			// 寿命をばらして撒き、一斉に消えないようにします。
			for (int i = 0; i < count; ++i)
			{
				EmitDust(focus, static_cast<int>(Random(0.2f, 1.0f) * kDustLifeFrames));
			}
			return;
		}
		// 粒の寿命はワールド時間で進むので、撒く量もワールド時間に合わせると、減速中も数が変わりません。
		const float lifeSeconds = kDustLifeFrames / 60.0f;
		g_DustCarry += static_cast<float>(count) / lifeSeconds * realDeltaTime * SlowClock::GetWorldScale();
		for (; g_DustCarry >= 1.0f; g_DustCarry -= 1.0f)
		{
			EmitDust(focus, kDustLifeFrames);
		}
	}

	void ApplyPostEffect()
	{
		const float intensity = SlowTimeFx::GetIntensity();
		const float wave = SlowTimeFx::GetWaveProgress();
		// 時間減速の演出が何も無いときも、仕上げ(接地の陰・ブルーム・ビネット)のため post-realistic は掛けたままにします。
		// 減速の開始・終了で付け外しをしないので、切り替わりで画面が跳びません。
		g_TimeSlowShown = intensity > 0.001f || wave > 0.0f || g_Flash > 0.0f;
		if (!SlowTimeFx::IsRealisticPostActive())
		{
			if (g_PostUnavailable || !Engine::Renderer::SetPostEffectDefinition(kPostEffectId))
			{
				g_PostUnavailable = true;
				g_TimeSlowShown = false;
				return;
			}
		}
		const SlowActionTuning& tuning = GetSlowActionTuning();
		Engine::Renderer::SetPostEffectFloat("aoStrength", tuning.PostAoStrength);
		Engine::Renderer::SetPostEffectFloat("aoRadius", tuning.PostAoRadius);
		Engine::Renderer::SetPostEffectFloat("bloomIntensity", tuning.PostBloomIntensity);
		Engine::Renderer::SetPostEffectFloat("bloomThreshold", tuning.PostBloomThreshold);
		// 時間減速の動的な値と見た目です。extra.z は Renderer が書く印なので空けておきます
		// (shaders/post/realisticCompositePS.hlsl)。
		Engine::Renderer::SetPostEffectParameters({ intensity, wave, g_Flash, g_WaveSign },
			{ tuning.SlowFxDesaturate, tuning.SlowFxTint, 0.0f, tuning.SlowFxVignette });
		Engine::Renderer::SetPostEffectLook({ tuning.PostVignette, 0.0f, 0.0f, 0.0f });
	}
}

void SlowTimeFx::Reset()
{
	g_WaveTime = -1.0f;
	g_WaveSign = 1.0f;
	g_Flash = 0.0f;
	g_FlashFresh = false;
	g_WasSlow = false;
	g_DustCarry = 0.0f;
	g_PostUnavailable = false;
	g_TimeSlowShown = false;
	g_Random.seed(20261120u);
	if (IsRealisticPostActive())
	{
		Engine::Renderer::SetPostEffect(Engine::PostEffectMode::None);
	}
}

void SlowTimeFx::BeginScene()
{
	// 新しいシーンでは粒を撒く場所が空なので、最初の更新で一括して撒き直します。
	// 編集モードや再生の切り替えでは粒は残っているので、撒き直さず Reset だけにします。
	Reset();
	g_DustWarmed = false;
}

void SlowTimeFx::Update(float realDeltaTime, const Vector3& focus)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	// 発動と解除(ゲージ切れの強制解除を含む)の瞬間に衝撃波を走らせます。
	const bool slow = SlowClock::IsSlowActive();
	if (slow != g_WasSlow)
	{
		g_WasSlow = slow;
		g_WaveSign = slow ? 1.0f : -1.0f;
		g_WaveTime = tuning.SlowFxWaveSeconds > 0.0f ? 0.0f : -1.0f;
	}
	if (g_WaveTime >= 0.0f)
	{
		g_WaveTime += realDeltaTime;
		if (g_WaveTime >= tuning.SlowFxWaveSeconds)
		{
			g_WaveTime = -1.0f;
		}
	}
	if (g_FlashFresh)
	{
		// 発動したフレームは最大値のまま。次のフレームから減衰を始めます。
		g_FlashFresh = false;
	}
	else if (g_Flash > 0.0f)
	{
		g_Flash = tuning.SlowFxFlashSeconds > 0.0f
			? (std::max)(0.0f, g_Flash - realDeltaTime / tuning.SlowFxFlashSeconds) : 0.0f;
	}
	UpdateDust(realDeltaTime, focus);
	ApplyPostEffect();
}

void SlowTimeFx::TriggerJustSlowFlash()
{
	if (GetSlowActionTuning().SlowFxFlashSeconds > 0.0f)
	{
		g_Flash = 1.0f;
		g_FlashFresh = true;
	}
}

float SlowTimeFx::GetIntensity()
{
	// SlowTimeScale が1以上(減速しない設定)なら、深さは常に0です。0で割らないようにします。
	const float depth = 1.0f - GetSlowActionTuning().SlowTimeScale;
	if (depth <= 0.001f)
	{
		return 0.0f;
	}
	return std::clamp((1.0f - SlowClock::GetWorldScale()) / depth, 0.0f, 1.0f);
}

float SlowTimeFx::GetWaveProgress()
{
	const float seconds = GetSlowActionTuning().SlowFxWaveSeconds;
	if (g_WaveTime < 0.0f || seconds <= 0.0f)
	{
		return 0.0f;
	}
	// 走り始めのフレームも「衝撃波あり」と分かるよう、0より少しだけ大きくします。
	return std::clamp(g_WaveTime / seconds, 1e-4f, 1.0f);
}

float SlowTimeFx::GetWaveSign() { return g_WaveSign; }
float SlowTimeFx::GetFlash() { return g_Flash; }

bool SlowTimeFx::IsPostEffectActive()
{
	return g_TimeSlowShown && IsRealisticPostActive();
}

bool SlowTimeFx::IsRealisticPostActive()
{
	return Engine::Renderer::GetPostEffectDefinitionId() == kPostEffectId;
}
