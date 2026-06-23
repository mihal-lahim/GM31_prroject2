#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "title.h"
#include "polygon2D.h"
#include "keyboard.h"
#include "game.h"
#include "result.h"
void Result::Init()
{
	Manager::AddGameObject<Polygon2D>()->Init(0.0f, 0.0f, 
		SCREEN_WIDTH, SCREEN_HEIGHT, L"assets/texture/result.png");
}

void Result::Uninit()
{}

void Result::Update()
{
	if (Keyboard_IsKeyDown(KK_ENTER))
	{
		Manager::ChangeScene<Title>();
	}
}

void Result::Draw()
{}
