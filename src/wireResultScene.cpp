#include "wireResultScene.h"

#include "alphabetText.h"
#include "gameTime.h"
#include "kanjiText.h"
#include "keyboard_utf8.h"
#include "main.h"
#include "polygon2D.h"
#include "score.h"
#include "wireGame.h"
#include "wireTitleScene.h"
#include "world.h"

#include <algorithm>
#include <cmath>

namespace
{
	int Centiseconds(float seconds)
	{
		return std::clamp(static_cast<int>(seconds * 100.0f), 0, 999999);
	}

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

	Score* AddTime(World& world, float seconds, const Vector3& position, const XMFLOAT4& color)
	{
		Score* score = world.Spawn<Score>();
		score->SetDigitCount(6);
		score->SetValue(Centiseconds(seconds));
		score->SetPosition(position);
		score->SetGlyphSize(58);
		score->SetGlyphAdvance(-48);
		score->SetColor(color);
		return score;
	}
}

void WireResultScene::Init()
{
	GameTime::SetPaused(false);
	World& world = GetWorld();
	world.Spawn<Polygon2D>()->Init(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, L"assets/texture/result.png");
	AddAlphabet(world, "ALL DELIVERED", { 270,135,0 }, 64, 50, { 1.0f,0.72f,0.18f,1.0f });
	AddAlphabet(world, "TIME", { 375,305,0 }, 42, 34, { 0.92f,0.97f,1.0f,1.0f });
	AddTime(world, m_ClearTime, { 890,295,0 }, { 0.92f,0.97f,1.0f,1.0f });
	AddAlphabet(world, "BEST", { 375,395,0 }, 42, 34, { 0.18f,0.86f,1.0f,1.0f });
	AddTime(world, m_BestTime, { 890,385,0 }, { 0.18f,0.86f,1.0f,1.0f });
	m_RetryPrompt = AddAlphabet(world, "PRESS R TO RETRY", { 375,570,0 }, 34, 27, { 0.92f,0.97f,1.0f,1.0f });
	m_TitlePrompt = AddAlphabet(world, "PRESS ENTER FOR TITLE", { 315,625,0 }, 28, 23, { 1.0f,0.82f,0.34f,1.0f });

	KanjiText* kanji = world.Spawn<KanjiText>();
	kanji->SetText(L"配達完了");
	kanji->SetPosition({ 535,235,0 });
	kanji->SetGlyphSize(42);
	kanji->SetGlyphAdvance(44);
	kanji->SetColor({ 1.0f,0.82f,0.34f,1.0f });
	m_AnimationTime = 0.0f;
}

void WireResultScene::Uninit() {}

void WireResultScene::Update()
{
	m_AnimationTime += GameTime::GetDeltaTime();
	const float pulse = 0.5f + 0.5f * sinf(m_AnimationTime * 3.6f);
	if (m_RetryPrompt != nullptr)
	{
		m_RetryPrompt->SetPosition({ 375.0f, 570.0f + sinf(m_AnimationTime * 2.0f) * 5.0f, 0.0f });
		m_RetryPrompt->SetColor({ 0.72f + pulse * 0.25f, 0.84f + pulse * 0.13f, 1.0f, 0.62f + pulse * 0.38f });
	}
	if (m_TitlePrompt != nullptr)
	{
		m_TitlePrompt->SetColor({ 1.0f, 0.68f + pulse * 0.18f, 0.22f, 0.58f + pulse * 0.42f });
	}
	if (Keyboard_IsKeyDownTrigger(KK_R))
	{
		ChangeScene<WireGame>();
	}
	else if (Keyboard_IsKeyDownTrigger(KK_ENTER))
	{
		ChangeScene<WireTitleScene>();
	}
}

void WireResultScene::Draw() {}
