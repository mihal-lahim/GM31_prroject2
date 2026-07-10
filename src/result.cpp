#include "main.h"
#include "renderer.h"
#include "title.h"
#include "polygon2D.h"
#include "keyboard_utf8.h"
#include "game.h"
#include "result.h"
void Result::Init()
{
	GetWorld().Spawn<Polygon2D>()->Init(0.0f, 0.0f,
		SCREEN_WIDTH, SCREEN_HEIGHT, L"assets/texture/result.png");
}

void Result::Uninit()
{}

void Result::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_ENTER))
	{
		ChangeScene<Title>();
	}
}

void Result::Draw()
{}
