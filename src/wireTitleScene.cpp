#include "wireTitleScene.h"

#include "alphabetText.h"
#include "gameTime.h"
#include "kanaText.h"
#include "kanjiText.h"
#include "keyboard_utf8.h"
#include "main.h"
#include "polygon2D.h"
#include "wireGame.h"
#include "world.h"

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
	AddAlphabet(world, "WIRE DELIVERY", { 250,170,0 }, 72, 56, { 1.0f,0.72f,0.18f,1.0f });
	AddAlphabet(world, "PRESS ENTER", { 430,470,0 }, 42, 34, { 0.92f,0.97f,1.0f,1.0f });
	AddAlphabet(world, "WASD MOVE SHIFT DASH", { 365,570,0 }, 28, 23, { 0.18f,0.86f,1.0f,1.0f });

	KanaText* kana = world.Spawn<KanaText>();
	kana->SetText(L"ワイヤーアクション");
	kana->SetPosition({ 405,300,0 });
	kana->SetGlyphSize(45);
	kana->SetGlyphAdvance(43);
	kana->SetColor({ 0.94f,0.97f,1.0f,1.0f });

	KanjiText* kanji = world.Spawn<KanjiText>();
	kanji->SetText(L"配達開始");
	kanji->SetPosition({ 555,375,0 });
	kanji->SetGlyphSize(38);
	kanji->SetGlyphAdvance(42);
	kanji->SetColor({ 1.0f,0.82f,0.34f,1.0f });
}

void WireTitleScene::Uninit() {}

void WireTitleScene::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_ENTER)) ChangeScene<WireGame>();
}

void WireTitleScene::Draw() {}
