#include"main.h"
#include "manager.h"
#include "gameTime.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "gameObject.h"
#include "bullet.h"
#include "player.h"
#include "enemy.h"
#include "explosion.h"
#include "score.h"


void Bullet::Init()
{
	m_Layer = Layer::World;
	AddComponent<ModelRenderer>(this)->Load("assets\\model\\bullet.obj");
	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(&m_PixelShader, "unlitTexturePS.cso");

}

void Bullet::Uninit()
{
	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
}

void Bullet::Update()
{
	float dt = GameTime::GetDeltaTime();//1/60秒に動く数値を指定するためE
	float speed = 1.5f;//そ�E数値
	//摩擦
	float friction = 0.9f;
	//減送E
	m_Velocity *= friction;
	//移動値適用
	m_Position += m_Velocity;

	//敵との衝突判宁E
	auto enemies = Manager::GetGameObjects<Enemy>();
	for (auto* enemy : enemies)
	{
		Vector3 direction = enemy->GetPosition() - m_Position;
		float length = direction.length();

		if (length < 1.0f)//衝突判定　距離ぁE.0f未満なら衝突とみなぁE
		{
			enemy->SetDestroy();//敵を削除予紁E
			SetDestroy();//自刁E�E身を削除予紁E

			Manager::AddGameObject<Explosion>()->SetPosition(enemy->GetPosition()+Vector3(0.0f, 1.0f, 0.0f));//爁E��エフェクト生戁E
			Manager::GetGameObject<Score>()->Add(1); // スコアを追加

			break;//ループから抜ける
		}

	}

	//寿命減封E
	m_Lifetime -= dt;
	if (m_Lifetime <= 0.0f)
	{
		//自刁E�E身を削除予紁E
		SetDestroy();
	}
	//継承允E�EUpdateを呼び出ぁE
	GameObject::Update();
}

void Bullet::Draw()
{
	//入力レイアウト設宁E
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設宁E
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);


	//マトリクス設宁E
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	//継承允E�EDrawを呼び出ぁE
	GameObject::Draw();
}


