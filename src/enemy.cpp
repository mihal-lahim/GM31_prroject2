#include"main.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "enemy.h"
#include "player.h"
#include "collider.h"



void Enemy::Init()
{
	m_Layer = Layer::World;

	AddComponent<ModelRenderer>()->Load("assets\\model\\player.obj");

	AddComponent<Collider>()
		->SetSphere(0.8f)
		->SetOffset({ 0.0f, 0.8f, 0.0f })
		->SetTag(Collider::Tag::Enemy)
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
	m_Position += m_Velocity;

	GameObject::Update();
}

void Enemy::Draw()
{
	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}

