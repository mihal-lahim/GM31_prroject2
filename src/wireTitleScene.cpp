#include "wireTitleScene.h"

#include "alphabetText.h"
#include "gameTime.h"
#include "kanaText.h"
#include "kanjiText.h"
#include "keyboard_utf8.h"
#include "main.h"
#include "polygon2D.h"
#include "dynamicText.h"
#include "wireDebugTools.h"
#include "wireGame.h"
#include "wireResultScene.h"
#include "wireTutorialScene.h"
#include "world.h"

#include <cmath>

namespace
{
	AlphabetText* AddAlphabet(World& world, const char* text, const Vector3& position,
		float size, float advance, const XMFLOAT4& color)
	{
		AlphabetText* label = world.Spawn<AlphabetText>();
		label->SetText(text);
		label->SetPosition(position);
		label->SetGlyphSize(size);
		label->SetGlyphAdvance(advance);
		label->SetColor(color);
		return label;
	}
}

void WireTitleScene::Init()
{
	GameTime::SetPaused(false);
	World& world = GetWorld();
	world.Spawn<Polygon2D>()->Init(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, L"assets/texture/title.png");
	AddAlphabet(world, "STEEL RESCUE", { 285,140,0 }, 76, 58, { 1.0f,0.72f,0.18f,1.0f });

	// 一言で内容を伝えます。長い英文を並べると読まれないため、日本語の短い一行にします。
	auto addLine = [&world](const std::wstring& text, float y, int size, const XMFLOAT4& color)
	{
		DynamicText* line = world.Spawn<DynamicText>();
		line->SetText(text);
		line->SetAlign(DynamicText::Align::Center);
		line->SetPosition({ SCREEN_WIDTH * 0.5f, y, 0.0f });
		line->SetFontSize(size);
		line->SetColor(color);
		return line;
	};
	addLine(L"暴走した工場をワイヤーで駆け抜け、作業員を救え", 246.0f, 26, { 0.55f,0.90f,1.0f,1.0f });
	// 練習場で一通り操作を身につけてから本編へ進む一本道です。操作表はここには出しません。
	m_StartLine = addLine(L"Enter で はじめる", 470.0f, 36, { 0.92f,0.97f,1.0f,1.0f });
	addLine(L"最初に練習場で操作を覚えてから、工場へ向かいます", 530.0f, 20, { 0.75f,0.85f,0.95f,0.9f });

	KanaText* kana = world.Spawn<KanaText>();
	kana->SetText(L"コウジョウキュウシュツ");
	kana->SetPosition({ 355,330,0 });
	kana->SetGlyphSize(42);
	kana->SetGlyphAdvance(40);
	kana->SetColor({ 0.94f,0.97f,1.0f,1.0f });

	KanjiText* kanji = world.Spawn<KanjiText>();
	kanji->SetText(L"救出開始");
	kanji->SetPosition({ 555,395,0 });
	kanji->SetGlyphSize(38);
	kanji->SetGlyphAdvance(42);
	kanji->SetColor({ 1.0f,0.82f,0.34f,1.0f });
	// 審査時にデバッグ機能へ到達できるよう、有効化キーを常時表示します。
	AddAlphabet(world, "F1 DEBUG MENU", { 24, SCREEN_HEIGHT - 40.0f, 0 }, 20, 16,
		{ 0.45f,1.0f,0.58f,0.72f });
	m_AnimationTime = 0.0f;
}

void WireTitleScene::Uninit() {}

void WireTitleScene::Update()
{
	switch (WireDebugTools::Update())
	{
	case WireDebugTools::SceneRequest::Game: ChangeScene<WireGame>(); return;
	case WireDebugTools::SceneRequest::ResultCleared:
		ChangeScene<WireResultScene>(0.0f, 0.0f, true); return;
	case WireDebugTools::SceneRequest::ResultGameOver:
		ChangeScene<WireResultScene>(0.0f, 0.0f, false); return;
	default: break;
	}

	m_AnimationTime += GameTime::GetDeltaTime();
	if (m_StartLine != nullptr)
	{
		const float pulse = 0.5f + 0.5f * sinf(m_AnimationTime * 4.0f);
		m_StartLine->SetPosition({ SCREEN_WIDTH * 0.5f, 462.0f + sinf(m_AnimationTime * 2.0f) * 5.0f, 0.0f });
		m_StartLine->SetColor({ 0.60f + pulse * 0.32f, 0.82f + pulse * 0.15f, 1.0f, 0.55f + pulse * 0.45f });
	}
	// 必ず練習場を通ります。本編へ直行する手段は開発用のF8だけです。
	if (Keyboard_IsKeyDownTrigger(KK_ENTER)) ChangeScene<WireTutorialScene>();
}

void WireTitleScene::Draw() {}
