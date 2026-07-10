#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "collider.h"
#include "battingCollisionLayers.h"
#include "battingWall.h"

void BattingWall::Init()
{
	m_Layer = Layer::World;
	AddComponent<ModelRenderer>()
		->SetUseOwnerTransform()
		->Load("assets\\model\\box.obj");
	AddComponent<Collider>()
		->SetAABB({ 1.0f, 1.0f, 1.0f })
		->SetCollisionLayer(BattingCollisionLayers::Wall)
		->SetStatic()
		->SetDebugColor(XMFLOAT4(1.0f, 0.25f, 1.0f, 1.0f));
}

void BattingWall::Uninit()
{
	GameObject::Uninit();
}

void BattingWall::Update()
{
	GameObject::Update();
}

void BattingWall::Draw()
{
	GameObject::Draw();
}
