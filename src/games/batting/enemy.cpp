#include"main.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "enemy.h"
#include "player.h"
#include "collider.h"
#include "battingCollisionLayers.h"



void Enemy::Init()
{
	m_Layer = Layer::World;

	AddComponent<ModelRenderer>()->Load("assets\\model\\player.obj");

	AddComponent<Collider>()
		->SetSphere(0.8f)
		->SetOffset({ 0.0f, 0.8f, 0.0f })
		->SetCollisionLayer(BattingCollisionLayers::Enemy)
		->SetDebugColor(XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f));

}

void Enemy::Uninit()
{
	GameObject::Uninit();
}

void Enemy::Update()
{
	float friction = 0.9f;
	// 速度を減衰させる。
	m_Velocity *= friction;
	// 速度を座標に反映する。
	SetPosition(GetPosition() + (m_Velocity));

	GameObject::Update();
}

void Enemy::Draw()
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

