
#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "player.h"
#include "gameTime.h"
#include "audio.h"
#include "playerMoveParticle.h"
#include "collider.h"
#include "battingCollisionLayers.h"


void Player::Init()
{
	m_Layer = Layer::World;
	SetPosition({ 0.0f, 0.0f, 0.0f });

	AddComponent<ModelRenderer>()->Load("assets\\model\\player.obj");
	
	// ジャンプ時のSEを読み込む。
	m_JumpSE = AddComponent<Audio>();
	m_JumpSE->Load("assets/audio/wan.wav");
	AddComponent<PlayerMoveParticle>();
	AddComponent<Collider>()
		->SetCapsule(0.45f, 1.8f)
		->SetOffset({ 0.0f, 0.9f, 0.0f })
		->SetCollisionLayer(BattingCollisionLayers::Player)
		->SetDebugColor(XMFLOAT4(0.2f, 0.8f, 1.0f, 1.0f));
}

void Player::Uninit()
{
	GameObject::Uninit();
}

void Player::Update()
{
	Vector3 oldPosition = GetPosition();
	float dt = Engine::GameTime::GetDeltaTime();

	UpdateMovement(dt);
	UpdateCombat(dt);
	ResolveLegacyCollisions(oldPosition);

	GameObject::Update();
}

void Player::Draw()
{
	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(GetScale().x, GetScale().y, GetScale().z);
	rot = XMMatrixRotationRollPitchYaw(GetRotation().x, GetRotation().y + XM_PI, GetRotation().z);
	trans = XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);
	world = scale * rot * trans;

	Engine::Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}






