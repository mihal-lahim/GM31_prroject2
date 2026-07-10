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


void Bullet::Init()
{
	m_Layer = Layer::World;
	AddComponent<ModelRenderer>()->Load("assets\\model\\bullet.obj");

	m_Collider = AddComponent<Collider>();
	m_Collider->SetSphere(0.35f)
		->SetTag(Collider::Tag::Bullet)
		->SetDebugColor(XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f));

}

void Bullet::Uninit()
{
	GameObject::Uninit();
}

void Bullet::Update()
{
	float dt = GameTime::GetDeltaTime();// 1フレーム分の経過時間
	float friction = 0.9f;
	// 速度を減衰させる。
	m_Velocity *= friction;
	// 速度を座標に反映する。
	m_Position += m_Velocity;

	// 敵との衝突判定。
	auto hitColliders = Collider::GetOverlaps(*m_Collider, Collider::Tag::Enemy);
	for (Collider* collider : hitColliders)
	{
		Enemy* enemy = dynamic_cast<Enemy*>(collider->GetOwner());
		if (enemy != nullptr)
		{
			enemy->SetDestroy();// 敵を削除予約する。
			SetDestroy();// 弾自身を削除予約する。

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
		SetDestroy();
	}
	GameObject::Update();
}

void Bullet::Draw()
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


