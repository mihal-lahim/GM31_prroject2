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
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}

