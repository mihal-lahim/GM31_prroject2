#include"main.h"
#include "manager.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "gameObject.h"
#include "bullet.h"
#include "player.h"
#include "enemy.h"
#include "explosion.h"



void Bullet::Init()
{
	m_Layer = 1;
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
	float dt = 1.0f / 60.0f;//1/60秒に動く数値を指定するため
	float speed = 1.5f;//その数値
	//摩擦
	float friction = 0.9f;
	//減速
	m_Velocity *= friction;
	//移動値適用
	m_Position += m_Velocity;

	//敵との衝突判定
	auto enemies = Manager::GetGameObjects<Enemy>();
	for (auto* enemy : enemies)
	{
		Vector3 direction = enemy->GetPosition() - m_Position;
		float length = direction.length();

		if (length < 1.0f)//衝突判定　距離が1.0f未満なら衝突とみなす
		{
			enemy->SetDestroy();//敵を削除予約
			SetDestroy();//自分自身を削除予約

			Manager::AddGameObject<Explosion>()->SetPosition(enemy->GetPosition()+Vector3(0.0f, 1.0f, 0.0f));//爆発エフェクト生成

			break;//ループから抜ける
		}

	}

	//寿命減少
	m_Lifetime -= dt;
	if (m_Lifetime <= 0.0f)
	{
		//自分自身を削除予約
		SetDestroy();
	}
	//継承元のUpdateを呼び出す
	GameObject::Update();
}

void Bullet::Draw()
{
	//入力レイアウト設定
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設定
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);


	//マトリクス設定
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	//継承元のDrawを呼び出す
	GameObject::Draw();
}
