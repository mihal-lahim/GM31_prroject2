#include "soundPool.h"
#include "slowAction.h"

#include "slowActionTuning.h"
#include "audio.h"

// スコア・コンボと効果音です。どちらも「何が起きたか」を受け取って、遊んでいる人へ返す部分です。

namespace
{
	int g_Score = 0;
	int g_BestScore = 0;
	int g_Combo = 0;
	int g_BestCombo = 0;
	float g_ComboTime = 0.0f;
	std::vector<SlowScore::Popup> g_Popups;
	SlowSoundBank* g_Bank = nullptr;

	constexpr float kPopupSeconds = 1.4f;
	constexpr std::size_t kMaxPopups = 5;

	const char* SoundPath(SlowSound::Id id)
	{
		switch (id)
		{
		case SlowSound::Id::Hit: return "assets\\audio\\slowHit.wav";
		case SlowSound::Id::StrongHit: return "assets\\audio\\slowStrongHit.wav";
		case SlowSound::Id::Bounce: return "assets\\audio\\slowBounce.wav";
		case SlowSound::Id::Kill: return "assets\\audio\\slowKill.wav";
		case SlowSound::Id::Damage: return "assets\\audio\\slowDamage.wav";
		case SlowSound::Id::TimeOn: return "assets\\audio\\slowTimeOn.wav";
		case SlowSound::Id::Shot: return "assets\\audio\\slowShot.wav";
		case SlowSound::Id::Deflect: return "assets\\audio\\slowDeflect.wav";
		default: return nullptr;
		}
	}

	// コンボを伸ばす出来事か(壁反射と全滅ボーナスは得点だけで、数は増やしません)。
	bool CountsAsHit(SlowScore::Event event)
	{
		switch (event)
		{
		case SlowScore::Event::NormalHit:
		case SlowScore::Event::StrongHit:
		case SlowScore::Event::Pursuit:
		case SlowScore::Event::Deflect:
			return true;
		default:
			return false;
		}
	}
}

void SlowScore::Reset()
{
	g_Score = 0;
	g_Combo = 0;
	g_ComboTime = 0.0f;
	g_Popups.clear();
}

void SlowScore::Update(float realDeltaTime, float worldDeltaTime)
{
	if (g_Combo > 0)
	{
		g_ComboTime -= worldDeltaTime;
		if (g_ComboTime <= 0.0f)
		{
			g_Combo = 0;
			g_ComboTime = 0.0f;
		}
	}
	for (Popup& popup : g_Popups)
	{
		popup.Age += realDeltaTime;
	}
	std::erase_if(g_Popups, [](const Popup& popup) { return popup.Age >= kPopupSeconds; });
}

float SlowScore::GetMultiplier()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const int every = (std::max)(tuning.ComboMultiplierEvery, 1);
	return (std::min)(1.0f + static_cast<float>(g_Combo / every) * tuning.ComboMultiplierStep, tuning.ComboMultiplierMax);
}

void SlowScore::Add(Event event)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	int base = 0;
	const wchar_t* label = L"";
	switch (event)
	{
	case Event::NormalHit: base = tuning.ScoreNormalHit; label = L"ヒット"; break;
	case Event::StrongHit: base = tuning.ScoreStrongHit; label = L"強打"; break;
	case Event::Pursuit: base = tuning.ScorePursuit; label = L"追撃"; break;
	case Event::WallBounce: base = tuning.ScoreWallBounce; label = L"壁反射"; break;
	case Event::Deflect: base = tuning.ScoreDeflect; label = L"打ち返し"; break;
	case Event::JustSlow: base = tuning.ScoreJustSlow; label = L"ジャスト減速"; break;
	case Event::KillMelee: base = tuning.ScoreKillMelee; label = L"撃破"; break;
	case Event::KillGunner: base = tuning.ScoreKillGunner; label = L"射撃型 撃破"; break;
	case Event::KillBrute: base = tuning.ScoreKillBrute; label = L"重量型 撃破"; break;
	case Event::Clear: base = tuning.ScoreClearBonus; label = L"全滅ボーナス"; break;
	}

	if (CountsAsHit(event))
	{
		g_Combo++;
		g_BestCombo = (std::max)(g_BestCombo, g_Combo);
	}
	// 壁反射や撃破もコンボの猶予を延ばします。吹き飛ばしを繋いでいる間は切れません。
	if (g_Combo > 0)
	{
		g_ComboTime = tuning.ComboWindowSeconds;
	}

	// 全滅ボーナスには倍率を掛けません。
	const float multiplier = event == Event::Clear ? 1.0f : GetMultiplier();
	const int points = static_cast<int>(static_cast<float>(base) * multiplier + 0.5f);
	g_Score += points;
	g_BestScore = (std::max)(g_BestScore, g_Score);

	// 小さな得点が続くと流れて読めないので、ヒットは表示せず、目立つ出来事だけを並べます。
	if (event == Event::NormalHit || event == Event::StrongHit)
	{
		return;
	}
	wchar_t text[64]{};
	swprintf_s(text, L"%s  +%d", label, points);
	g_Popups.insert(g_Popups.begin(), Popup{ text, 0.0f });
	if (g_Popups.size() > kMaxPopups)
	{
		g_Popups.pop_back();
	}
}

void SlowScore::BreakCombo()
{
	g_Combo = 0;
	g_ComboTime = 0.0f;
}

int SlowScore::GetScore() { return g_Score; }
int SlowScore::GetBestScore() { return g_BestScore; }
int SlowScore::GetCombo() { return g_Combo; }
int SlowScore::GetBestCombo() { return g_BestCombo; }
const std::vector<SlowScore::Popup>& SlowScore::GetPopups() { return g_Popups; }

float SlowScore::GetComboRemaining()
{
	const float window = GetSlowActionTuning().ComboWindowSeconds;
	return g_Combo > 0 && window > 0.0f ? std::clamp(g_ComboTime / window, 0.0f, 1.0f) : 0.0f;
}

const wchar_t* SlowScore::GetRank()
{
	if (g_Combo >= 25) return L"SS";
	if (g_Combo >= 16) return L"S";
	if (g_Combo >= 10) return L"A";
	if (g_Combo >= 6) return L"B";
	if (g_Combo >= 3) return L"C";
	return L"";
}

void SlowSoundBank::Init()
{
	for (int id = 0; id < static_cast<int>(SlowSound::Id::Count); ++id)
	{
		m_Pools[id] = AddComponent<SoundPool>(SoundPath(static_cast<SlowSound::Id>(id)), kVoices);
	}
	g_Bank = this;
}

void SlowSoundBank::Uninit()
{
	if (g_Bank == this)
	{
		g_Bank = nullptr;
	}
}

void SlowSoundBank::Play(SlowSound::Id id)
{
	const int index = static_cast<int>(id);
	if (index < 0 || index >= static_cast<int>(SlowSound::Id::Count))
	{
		return;
	}
	// 声を順番に使い回し、続けて鳴らしても前の音を途中で切らないようにします。
	m_Pools[index]->Play();
}

void SlowSound::Play(Id id)
{
	if (g_Bank != nullptr)
	{
		g_Bank->Play(id);
	}
}
