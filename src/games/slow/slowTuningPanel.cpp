#include "slowTuningPanel.h"

#include "main.h"
#include "third_party/imgui/imgui.h"
#include "collider.h"
#include "gameTime.h"
#include "runtimeConfig.h"
#include "sceneEditor.h"
#include "slowAction.h"
#include "slowActionTuning.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace
{
	// 設定ファイルの場所です(SlowActionGameDefinitionの設定ファイルと同じ)。
	constexpr const char* kConfigPath = "assets/config/slow-gameplay.cfg";
	constexpr const char* kProjectMarker = "SlowAction.vcxproj";

	bool g_Open = false;
	bool g_RetryRequested = false;
	bool g_RefillHpRequested = false;
	bool g_FillGaugeRequested = false;
	// 「ライトと画面の見た目」の見出しの名前です(.def の SLOW_TUNING_SECTION と同じ)。
	constexpr const char* kLightingSection = "ライトと画面の見た目";

	struct PresetValue
	{
		const char* Key;
		float Value;
	};
	// 夕暮れ: 低い太陽・暖色・長い影。ブルームを少し強め、外周を締めます。
	constexpr PresetValue kSunsetPreset[] = {
		{ "slow.SunAzimuthDegrees", 250.0f }, { "slow.SunElevationDegrees", 14.0f },
		{ "slow.SunIntensity", 1.15f }, { "slow.SunWarmth", 0.95f },
		{ "slow.SkyIntensity", 0.7f }, { "slow.GroundBounceIntensity", 1.2f },
		{ "slow.PostAoStrength", 0.6f }, { "slow.PostAoRadius", 0.7f },
		{ "slow.PostBloomIntensity", 0.5f }, { "slow.PostBloomThreshold", 0.85f }, { "slow.PostVignette", 0.22f },
	};
	// 寒色シネマティック: 青白い光と強めの空の光、深い陰と濃いビネットで、締まった映画調にします。
	constexpr PresetValue kCoolCinematicPreset[] = {
		{ "slow.SunAzimuthDegrees", 110.0f }, { "slow.SunElevationDegrees", 32.0f },
		{ "slow.SunIntensity", 0.85f }, { "slow.SunWarmth", 0.08f },
		{ "slow.SkyIntensity", 1.15f }, { "slow.GroundBounceIntensity", 0.7f },
		{ "slow.PostAoStrength", 0.7f }, { "slow.PostAoRadius", 0.7f },
		{ "slow.PostBloomIntensity", 0.42f }, { "slow.PostBloomThreshold", 0.9f }, { "slow.PostVignette", 0.26f },
	};
	// 保存・読み直しの結果を表示する一文です。
	std::string g_Status;
	char g_FilterText[64] = "";

	// 項目の既定値の範囲です。TuningFields::Read と同じ範囲に揃えて、画面の値と実際の値を一致させます。
	void ValueRange(float defaultValue, float& minimum, float& maximum)
	{
		const float magnitude = (std::max)(std::fabs(defaultValue) * 10.0f, 1.0f);
		minimum = defaultValue < 0.0f ? -magnitude : 0.0f;
		maximum = magnitude;
	}

	int IntMaximum(int defaultValue)
	{
		return (std::max)(std::abs(defaultValue) * 10, 100);
	}

	bool ContainsIgnoreCase(const std::string& text, const std::string& filter)
	{
		if (filter.empty())
		{
			return true;
		}
		const auto lower = [](std::string value)
		{
			for (char& c : value)
			{
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return value;
		};
		return lower(text).find(lower(filter)) != std::string::npos;
	}

	// 設定ファイルへ書く値の文字列です。小数は無駄な0を抑え、整数は整数で書きます。
	std::string FormatFloat(float value)
	{
		char text[64];
		std::snprintf(text, sizeof(text), "%.6g", static_cast<double>(value));
		return text;
	}

	std::string FormatInt(int value)
	{
		return std::to_string(value);
	}

	// いま有効な値を、設定ファイルへ書く形で全項目ぶん集めます。
	std::vector<std::pair<std::string, std::string>> CollectEntries()
	{
		std::vector<std::pair<std::string, std::string>> entries;
		for (const SlowTuningEntry& entry : GetSlowActionTuningEntries())
		{
			if (entry.Type == SlowTuningEntry::Kind::Float)
			{
				entries.emplace_back(entry.Key,
					FormatFloat(Engine::RuntimeConfig::GetFloat(entry.Key, entry.DefaultValue)));
			}
			else if (entry.Type == SlowTuningEntry::Kind::Int)
			{
				const float value = Engine::RuntimeConfig::GetFloat(entry.Key, entry.DefaultValue);
				entries.emplace_back(entry.Key, FormatInt(static_cast<int>(std::lround(value))));
			}
		}
		return entries;
	}

	void SaveAll()
	{
		const auto entries = CollectEntries();
		const std::string sourceText = [&]()
		{
			const std::filesystem::path source = SceneEditor::FindSourcePath(kConfigPath, kProjectMarker);
			return source.empty() ? std::string() : source.string();
		}();
		const bool runtimeSaved = Engine::RuntimeConfig::SavePreservingComments(kConfigPath, entries);
		bool sourceSaved = true;
		if (!sourceText.empty())
		{
			sourceSaved = Engine::RuntimeConfig::SavePreservingComments(sourceText, entries);
		}
		if (runtimeSaved && sourceSaved)
		{
			g_Status = sourceText.empty()
				? "保存しました(実行フォルダの設定ファイル)。"
				: "保存しました(実行フォルダとソースフォルダの両方)。次のビルドでも残ります。";
		}
		else
		{
			g_Status = "保存に失敗しました。ファイルが書き込み可能か確かめてください。";
		}
	}

	void ReloadAll()
	{
		const std::filesystem::path source = SceneEditor::FindSourcePath(kConfigPath, kProjectMarker);
		const std::filesystem::path path = !source.empty() && std::filesystem::exists(source) ? source
			: std::filesystem::path(kConfigPath);
		g_Status = Engine::RuntimeConfig::Load(path)
			? "読み直しました: " + path.string()
			: "読み直せませんでした: " + path.string();
	}

	void ResetAll()
	{
		// 既定値を明示的に書くので、設定ファイルにしか無いキーがあってもそのまま残ります。
		for (const SlowTuningEntry& entry : GetSlowActionTuningEntries())
		{
			if (entry.Type == SlowTuningEntry::Kind::Float)
			{
				Engine::RuntimeConfig::SetFloat(entry.Key, entry.DefaultValue);
			}
			else if (entry.Type == SlowTuningEntry::Kind::Int)
			{
				Engine::RuntimeConfig::SetInt(entry.Key, static_cast<int>(entry.DefaultValue));
			}
		}
		g_Status = "すべて既定値に戻しました(保存するまで設定ファイルは変わりません)。";
	}

	void DrawPlayback()
	{
		ImGui::TextUnformatted("時間の制御");
		const bool paused = Engine::GameTime::IsPaused();
		if (ImGui::Button(paused ? "再開" : "一時停止"))
		{
			Engine::GameTime::SetPaused(!paused);
		}
		ImGui::SameLine();
		// コマ送りは止まっているときだけ使えます。剣閃や当たりを1フレームずつ確かめるためです。
		ImGui::BeginDisabled(!paused);
		if (ImGui::Button("1フレーム進める"))
		{
			Engine::GameTime::RequestStepFrame();
		}
		ImGui::EndDisabled();

		float timeScale = Engine::GameTime::GetTimeScale();
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::SliderFloat("時間の速さ", &timeScale, 0.1f, 1.5f, "%.2fx"))
		{
			Engine::GameTime::SetTimeScale(timeScale);
		}
		ImGui::SameLine();
		if (ImGui::Button("1.0x"))
		{
			Engine::GameTime::SetTimeScale(1.0f);
		}

		bool debugDraw = Collider::GetDebugDrawEnabled();
		if (ImGui::Checkbox("当たり判定 (F1)", &debugDraw))
		{
			Collider::SetDebugDrawEnabled(debugDraw);
		}
		ImGui::SameLine();
		bool invincible = SlowEnemy::IsInvincible();
		if (ImGui::Checkbox("敵無敵 (F2)", &invincible))
		{
			SlowEnemy::SetInvincible(invincible);
		}
		ImGui::SameLine();
		if (ImGui::Button("やり直す (R)"))
		{
			g_RetryRequested = true;
		}

		// 試す回数を増やすための近道です。シーンが次の更新で実行します。
		if (ImGui::SmallButton("HP全回復"))
		{
			SlowTuningPanel::RequestRefillHp();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("時間ゲージ満タン"))
		{
			SlowTuningPanel::RequestFillGauge();
		}

		// 画面の効果の見比べです。調整値の強さは書き換えないので、戻すと元の見た目に戻ります。
		bool ssao = SlowTimeFx::IsSsaoEnabled();
		if (ImGui::Checkbox("SSAO(接地の陰)", &ssao))
		{
			SlowTimeFx::SetSsaoEnabled(ssao);
		}
		ImGui::SameLine();
		bool bloom = SlowTimeFx::IsBloomEnabled();
		if (ImGui::Checkbox("ブルーム", &bloom))
		{
			SlowTimeFx::SetBloomEnabled(bloom);
		}
	}

	void DrawLightingPresets()
	{
		ImGui::TextUnformatted("プリセット:");
		ImGui::SameLine();
		if (ImGui::SmallButton("標準(昼)"))
		{
			SlowTuningPanel::ApplyLightingPreset(SlowTuningPanel::LightingPreset::Standard);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("夕暮れ"))
		{
			SlowTuningPanel::ApplyLightingPreset(SlowTuningPanel::LightingPreset::Sunset);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("寒色シネマティック"))
		{
			SlowTuningPanel::ApplyLightingPreset(SlowTuningPanel::LightingPreset::CoolCinematic);
		}
	}

	void DrawItem(const SlowTuningEntry& entry, SlowActionTuning& values)
	{
		const std::string name = std::string(entry.Key).substr(std::string("slow.").size());
		const std::string label = name + "##" + entry.Key;
		ImGui::PushID(entry.Key);
		if (entry.Type == SlowTuningEntry::Kind::Float)
		{
			float value = *entry.FloatMember(values);
			float minimum = 0.0f;
			float maximum = 0.0f;
			ValueRange(entry.DefaultValue, minimum, maximum);
			const bool modified = std::fabs(value - entry.DefaultValue) > 1e-6f;
			const float speed = (std::max)(std::fabs(maximum) * 0.002f, 0.0001f);
			ImGui::SetNextItemWidth(160.0f);
			if (ImGui::DragFloat((modified ? "* " + label : label).c_str(), &value, speed, minimum, maximum, "%.4f"))
			{
				Engine::RuntimeConfig::SetFloat(entry.Key, value);
			}
			if (modified)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("戻す"))
				{
					Engine::RuntimeConfig::SetFloat(entry.Key, entry.DefaultValue);
				}
			}
		}
		else if (entry.Type == SlowTuningEntry::Kind::Int)
		{
			int value = *entry.IntMember(values);
			const int defaultValue = static_cast<int>(entry.DefaultValue);
			const bool modified = value != defaultValue;
			ImGui::SetNextItemWidth(160.0f);
			if (ImGui::DragInt((modified ? "* " + label : label).c_str(), &value, 0.2f, 0, IntMaximum(defaultValue)))
			{
				Engine::RuntimeConfig::SetInt(entry.Key, value);
			}
			if (modified)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("戻す"))
				{
					Engine::RuntimeConfig::SetInt(entry.Key, defaultValue);
				}
			}
		}
		ImGui::PopID();
	}

	void DrawTuningList()
	{
		ImGui::SetNextItemWidth(260.0f);
		ImGui::InputText("検索(キー名)", g_FilterText, sizeof(g_FilterText));
		const std::string filter = g_FilterText;

		// 見出しごとに、絞り込みに合う項目をまとめます。値は今の調整値を1回だけ写します。

		std::vector<std::pair<const char*, std::vector<const SlowTuningEntry*>>> sections;
		for (const SlowTuningEntry& entry : GetSlowActionTuningEntries())
		{
			if (entry.Type == SlowTuningEntry::Kind::Section)
			{
				sections.push_back({ entry.Key, {} });
			}
			else if (!sections.empty() && ContainsIgnoreCase(entry.Key, filter))
			{
				sections.back().second.push_back(&entry);
			}
		}

		SlowActionTuning editable = GetSlowActionTuning();
		for (const auto& [title, items] : sections)
		{
			if (items.empty())
			{
				continue;
			}
			// 絞り込み中は、合う項目のある見出しを必ず開きます(折りたたんだままだと結果が見えないため)。
			if (!filter.empty())
			{
				ImGui::SetNextItemOpen(true, ImGuiCond_Always);
			}
			if (ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (std::string(title) == kLightingSection)
				{
					DrawLightingPresets();
				}
				for (const SlowTuningEntry* entry : items)
				{
					DrawItem(*entry, editable);
				}
			}
		}
	}
}

void SlowTuningPanel::Toggle()
{
	if (g_Open)
	{
		Reset();
		return;
	}
	g_Open = true;
	// 開いている間は、マウスを画面から離して自由に動かせるようにします。
	// カーソルの切り替えは編集モード(F3)と合わせて SceneEditor がまとめて行います。
	SceneEditor::SetExternalCursorFree(true);
}

bool SlowTuningPanel::IsOpen()
{
	return g_Open;
}

void SlowTuningPanel::Reset()
{
	// 閉じる経路はここだけです(F4・ウィンドウの「×」・やり直し・シーンの終了)。
	// 編集モード中なら、カーソルは自由なまま残ります。
	g_Open = false;
	SceneEditor::SetExternalCursorFree(false);
	// 時間の制御を通常へ戻します。止めたまま・速いままでシーンを終えないようにします。
	Engine::GameTime::SetPaused(false);
	Engine::GameTime::SetTimeScale(1.0f);
	g_RetryRequested = false;
	g_RefillHpRequested = false;
	g_FillGaugeRequested = false;
	// 見比べのために切った画面の効果も戻します(切ったままシーンを終えないように)。
	SlowTimeFx::SetSsaoEnabled(true);
	SlowTimeFx::SetBloomEnabled(true);
}

void SlowTuningPanel::RequestRefillHp() { g_RefillHpRequested = true; }
void SlowTuningPanel::RequestFillGauge() { g_FillGaugeRequested = true; }

bool SlowTuningPanel::TakeRefillHpRequest()
{
	const bool requested = g_RefillHpRequested;
	g_RefillHpRequested = false;
	return requested;
}

bool SlowTuningPanel::TakeFillGaugeRequest()
{
	const bool requested = g_FillGaugeRequested;
	g_FillGaugeRequested = false;
	return requested;
}

void SlowTuningPanel::ApplyLightingPreset(LightingPreset preset)
{
	if (preset == LightingPreset::Standard)
	{
		// 標準は「ライトと画面の見た目」の全項目を既定値へ戻します(項目が増えてもそのまま効きます)。
		bool inSection = false;
		for (const SlowTuningEntry& entry : GetSlowActionTuningEntries())
		{
			if (entry.Type == SlowTuningEntry::Kind::Section)
			{
				inSection = std::string(entry.Key) == kLightingSection;
			}
			else if (inSection && entry.Type == SlowTuningEntry::Kind::Float)
			{
				Engine::RuntimeConfig::SetFloat(entry.Key, entry.DefaultValue);
			}
		}
		g_Status = "プリセット「標準(昼)」にしました(保存するまで設定ファイルは変わりません)。";
		return;
	}
	const bool sunset = preset == LightingPreset::Sunset;
	const auto apply = [](const auto& values)
	{
		for (const PresetValue& value : values)
		{
			Engine::RuntimeConfig::SetFloat(value.Key, value.Value);
		}
	};
	if (sunset)
	{
		apply(kSunsetPreset);
	}
	else
	{
		apply(kCoolCinematicPreset);
	}
	g_Status = sunset ? "プリセット「夕暮れ」にしました(保存するまで設定ファイルは変わりません)。"
		: "プリセット「寒色シネマティック」にしました(保存するまで設定ファイルは変わりません)。";
}

bool SlowTuningPanel::BlocksGameInput()
{
	if (!g_Open || ImGui::GetCurrentContext() == nullptr)
	{
		return false;
	}
	const ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureMouse || io.WantTextInput;
}

bool SlowTuningPanel::TakeRetryRequest()
{
	const bool requested = g_RetryRequested;
	g_RetryRequested = false;
	return requested;
}

void SlowTuningPanel::Draw()
{
	if (!g_Open || ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}
	// 左上のHUD(体力・減速ゲージ・状態)と重ならないよう、右側へ置きます。
	constexpr float kWidth = 470.0f;
	ImGui::SetNextWindowPos({ static_cast<float>(SCREEN_WIDTH) - kWidth - 16.0f, 16.0f }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({ kWidth, 640.0f }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.92f);
	// 「×」はImGuiが直接書き換えるので、ローカルの値で受けて、閉じる処理は Reset の1経路にまとめます。
	bool open = true;
	const bool visible = ImGui::Begin("調整 (F4)", &open);
	if (!visible)
	{
		ImGui::End();
		if (!open)
		{
			Reset();
		}
		return;
	}
	DrawPlayback();
	ImGui::Separator();
	if (ImGui::Button("保存"))
	{
		SaveAll();
	}
	ImGui::SameLine();
	if (ImGui::Button("ファイルから読み直す"))
	{
		ReloadAll();
	}
	ImGui::SameLine();
	if (ImGui::Button("すべて既定値に戻す"))
	{
		ResetAll();
	}
	if (!g_Status.empty())
	{
		ImGui::TextWrapped("%s", g_Status.c_str());
	}
	ImGui::Separator();
	DrawTuningList();
	ImGui::End();
	if (!open)
	{
		Reset();
	}
}
