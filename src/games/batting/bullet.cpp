#include"main.h"
#include "world.h"
#include "gameTime.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "gameObject.h"
#include "bullet.h"
#include "player.h"
#include "enemy.h"
#include "explosion.h"
#include "score.h"
#include "collider.h"
#include "battingCollisionLayers.h"


void Bullet::Init()
{
	m_Layer = Layer::World;
	AddComponent<ModelRenderer>()->Load("assets\\model\\bullet.obj");

	m_Collider = AddComponent<Collider>();
	m_Collider->SetSphere(0.35f)
		->SetCollisionLayer(BattingCollisionLayers::Bullet)
		->SetDebugColor(XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f));

}

void Bullet::Uninit()
{
	GameObject::Uninit();
}

void Bullet::Update()
{
	float dt = Engine::GameTime::GetDeltaTime();// 1フレーム分の経過時間
	float friction = 0.9f;
	// 速度を減衰させる。
	m_Velocity *= friction;
	// 速度を座標に反映する。
	SetPosition(GetPosition() + (m_Velocity));

	// 敵との衝突判定。
	auto hitColliders = m_Collider->GetOverlaps(BattingCollisionLayers::Enemy);
	for (Collider* collider : hitColliders)
	{
		Enemy* enemy = dynamic_cast<Enemy*>(collider->GetOwner());
		if (enemy != nullptr)
		{
			enemy->MarkForDestroy();// 敵を削除予約する。
			MarkForDestroy();// 弾自身を削除予約する。

			GetWorld().Spawn<Explosion>()->SetPosition(enemy->GetPosition()+Vector3(0.0f, 1.0f, 0.0f));// 爆発エフェクトを生成する。
			if (Score* score = GetWorld().FindFirst<Score>())
			{
				score->Add(1);
			}

			break;// 1体に当たったら判定を抜ける。
		}
	}

	// 寿命を減らす。
	m_Lifetime -= dt;
	if (m_Lifetime <= 0.0f)
	{
		// 寿命が切れたら自身を削除予約する。
		MarkForDestroy();
	}
	GameObject::Update();
}

void Bullet::Draw()
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


