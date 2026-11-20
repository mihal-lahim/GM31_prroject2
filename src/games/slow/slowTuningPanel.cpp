#include "slowTuningPanel.h"

#include "main.h"
#include "third_party/imgui/imgui.h"
#include "collider.h"
#include "gameTime.h"
#include "mouseInput.h"
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
			if (ImGui::CollapsingHeader(title, filter.empty() ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
			{
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
	Mouse_SetAutoCapture(false);
}

bool SlowTuningPanel::IsOpen()
{
	return g_Open;
}

void SlowTuningPanel::Reset()
{
	if (g_Open)
	{
		g_Open = false;
		Mouse_SetAutoCapture(true);
	}
	// 時間の制御を通常へ戻します。止めたまま・速いままでシーンを終えないようにします。
	Engine::GameTime::SetPaused(false);
	Engine::GameTime::SetTimeScale(1.0f);
	g_RetryRequested = false;
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
	ImGui::SetNextWindowPos({ 16.0f, 16.0f }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({ 560.0f, 640.0f }, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("調整 (F4)", &g_Open))
	{
		ImGui::End();
		if (!g_Open)
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
	if (!g_Open)
	{
		Reset();
	}
}
