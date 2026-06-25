#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "title.h"
#include "polygon2D.h"
#include "keyboard.h"
#include "game.h"
void Title::Init()
{
	Manager::AddGameObject<Polygon2D>()->Init(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, L"assets/texture/title.png");
}

void Title::Uninit()
{}

void Title::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_ENTER))
	{
		Manager::ChangeScene<Game>();
	}
}

void Title::Draw()
{}
