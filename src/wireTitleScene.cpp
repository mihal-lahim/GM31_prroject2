#include "wireTitleScene.h"

#include "alphabetText.h"
#include "gameTime.h"
#include "kanaText.h"
#include "kanjiText.h"
#include "keyboard_utf8.h"
#include "main.h"
#include "polygon2D.h"
#include "wireDebugTools.h"
#include "wireGame.h"
#include "wireResultScene.h"
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
	// ゲーム内容を一行で伝えます。タイトルだけでは何をする遊びか分からないためです。
	AddAlphabet(world, "WIRE THROUGH THE RUNAWAY FACTORY",
		{ 250,242,0 }, 26, 21, { 0.55f,0.90f,1.0f,1.0f });
	AddAlphabet(world, "CUT A PATH AND RESCUE THE WORKERS",
		{ 243,274,0 }, 26, 21, { 0.55f,0.90f,1.0f,1.0f });

	m_StartPrompt = AddAlphabet(world, "PRESS ENTER", { 430,470,0 }, 42, 34, { 0.92f,0.97f,1.0f,1.0f });
	// 移動を繋げる遊びであることを、操作表そのもので示します。
	AddAlphabet(world, "WASD MOVE   SPACE JUMP AND WALL KICK   SHIFT AIR DASH",
		{ 130,548,0 }, 22, 18, { 0.18f,0.86f,1.0f,1.0f });
	AddAlphabet(world, "RIGHT MOUSE AIM   LEFT MOUSE WIRE OR SWORD",
		{ 215,578,0 }, 22, 18, { 0.18f,0.86f,1.0f,1.0f });

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
	if (m_StartPrompt != nullptr)
	{
		const float pulse = 0.5f + 0.5f * sinf(m_AnimationTime * 4.0f);
		m_StartPrompt->SetPosition({ 430.0f, 470.0f + sinf(m_AnimationTime * 2.0f) * 7.0f, 0.0f });
		m_StartPrompt->SetColor({ 0.60f + pulse * 0.32f, 0.82f + pulse * 0.15f, 1.0f, 0.55f + pulse * 0.45f });
	}
	if (Keyboard_IsKeyDownTrigger(KK_ENTER)) ChangeScene<WireGame>();
}

void WireTitleScene::Draw() {}
