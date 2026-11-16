#include "box.h"
#include"main.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "player.h"
#include "collider.h"
#include "battingCollisionLayers.h"



void Box::Init()
{
	m_Layer = Layer::World;

	AddComponent<ModelRenderer>()->Load("assets\\model\\box.obj");
	AddComponent<Collider>()
		->SetAABB({ 1.0f, 1.0f, 1.0f })
		->SetCollisionLayer(BattingCollisionLayers::Box)
		->SetDebugColor(XMFLOAT4(1.0f, 0.55f, 0.0f, 1.0f));

}

void Box::Uninit()
{
	GameObject::Uninit();
}

void Box::Update()
{
	float friction = 0.9f;
	// 速度を減衰させる。
	m_Velocity *= friction;
	// 速度を座標に反映する。
	SetPosition(GetPosition() + (m_Velocity));

	GameObject::Update();
}

void Box::Draw()
{
	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(GetScale().x, GetScale().y, GetScale().z);
	rot = XMMatrixRotationRollPitchYaw(GetRotation().x, GetRotation().y, GetRotation().z);
	trans = XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);
	world = scale * rot * trans;

	Engine::Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}

