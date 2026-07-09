

#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "keyboard_utf8.h"
#include "player.h"
#include "bullet.h"
#include "manager.h"
#include "gameTime.h"
#include "camera.h"
#include "tree.h"
#include "box.h"
#include "audio.h"
#include "playerMoveParticle.h"
#include "jumpEffect.h"
#include "collider.h"
#include "baseballBat.h"

static float WrapAngle(float angle)
{
	while (angle > XM_PI)
	{
		angle -= XM_PI * 2.0f;
	}
	while (angle < -XM_PI)
	{
		angle += XM_PI * 2.0f;
	}
	return angle;
}

static float LerpAngle(float current, float target, float t)
{
	float delta = WrapAngle(target - current);
	return WrapAngle(current + delta * t);
}

// Turn response. 0.0f = no change, 1.0f = instant snap.
static constexpr float kTurnLerp = 0.22f;
// Shot interval in seconds. Lower values fire faster.
static constexpr float kShotCoolTime = 0.25f;


void Player::Init()
{
	m_Layer = Layer::World;
	m_Position = { 0.0f, 0.0f, 0.0f };

	AddComponent<ModelRenderer>(this)->Load("assets\\model\\player.obj");
	
	// モデル描画用のシェーダーを読み込む。
	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");
	// ジャンプ時のSEを読み込む。
	m_JumpSE = AddComponent<Audio>(this);
	m_JumpSE->Load("assets/audio/wan.wav");
	AddComponent<PlayerMoveParticle>(this);
	AddComponent<Collider>(this)
		->SetCapsule(0.45f, 1.8f)
		->SetOffset({ 0.0f, 0.9f, 0.0f })
		->SetTag(Collider::Tag::Player)
		->SetDebugColor(XMFLOAT4(0.2f, 0.8f, 1.0f, 1.0f));
}

void Player::Uninit()
{
	GameObject::Uninit();
}

void Player::Update()
{
	Vector3 oldPosition = m_Position;// 衝突時に戻すため、移動前の座標を保存する。

	float dt = GameTime::GetDeltaTime();// 1フレーム分の経過時間
	
	if (m_ShotCoolTime > 0.0f)
	{
		m_ShotCoolTime -= dt;
		if (m_ShotCoolTime < 0.0f)
		{
			m_ShotCoolTime = 0.0f;
		}
	}

	Camera* camera = Manager::GetGameObject<Camera>();
	Vector3 cameraForward{ 0.0f, 0.0f, 1.0f };
	Vector3 cameraRight{ 1.0f, 0.0f, 0.0f };
	if (camera != nullptr)
	{
		cameraForward = camera->GetForward();
		cameraForward.y = 0.0f;
		cameraForward.normalize();

		cameraRight = camera->GetRight();
		cameraRight.y = 0.0f;
		cameraRight.normalize();
	}

	Vector3 moveDirection{ 0.0f, 0.0f, 0.0f };

	// Movement input is camera-relative.
	if (Keyboard_IsKeyDown(KK_W))
	{
		moveDirection += cameraForward;
	}
	if (Keyboard_IsKeyDown(KK_S))
	{
		moveDirection -= cameraForward;
	}
	if (Keyboard_IsKeyDown(KK_A))
	{
		moveDirection -= cameraRight;
	}
	if (Keyboard_IsKeyDown(KK_D))
	{
		moveDirection += cameraRight;
	}

	float moveLength = moveDirection.length();
	if (moveLength > 0.0f)
	{
		moveDirection.y = 0.0f;
		moveDirection /= moveLength;

		// Rotate toward the movement direction with linear interpolation.
		float targetYaw = atan2f(moveDirection.x, moveDirection.z);
		m_Rotation.y = LerpAngle(m_Rotation.y, targetYaw, kTurnLerp);
		m_Velocity += moveDirection * 50.0f * dt;
	}
	else
	{
		
	}

	// 地面にいるときだけジャンプできる。
	if (Keyboard_IsKeyDownTrigger(KK_SPACE)&&GetOnGround())
	{
		m_Velocity.y +=20.0f;// 上方向へ速度を加える。

		m_Scale.y = 2.0f;// ジャンプ時の伸び縮み演出。
		m_Scale.x = 0.5f;
		m_Scale.z = 0.5f;
		m_JumpSE->Play();

		Vector3 effectPosition = m_Position;
		effectPosition.y = 0.0f;
		JumpEffect* jumpEffect = Manager::AddGameObject<JumpEffect>();
		jumpEffect->SetPosition(effectPosition);
		jumpEffect->EmitSparkles();
	}

	// 変形したスケールを少しずつ元に戻す。
	m_Scale.x += (1.0f - m_Scale.x) * 0.07f;
	m_Scale.y += (1.0f - m_Scale.y) * 0.07f;
	m_Scale.z += (1.0f - m_Scale.z) * 0.07f;

	// 重力を加える。
	m_Velocity.y += -50.0f * dt;
	// 水平方向の速度を減衰させる。
	m_Velocity.x += -m_Velocity.x * 5.0f * dt;
	m_Velocity.z += -m_Velocity.z * 5.0f * dt;

	// 速度を座標に反映する。
	m_Position += m_Velocity * dt;

	bool oldOnGround = GetOnGround();

	// 地面との衝突判定。
	if (m_Position.y < 0.0f)
	{
		m_Position.y = 0.0f;
		m_Velocity.y = 0.0f;
		SetOnGround(true);
	}
	else
	{
		SetOnGround(false);
	}

	// 着地した瞬間に押しつぶれたような演出を入れる。
	if (!oldOnGround && GetOnGround())
	{
		m_Scale.y = 0.8f;
		m_Scale.x = 1.5f;
		m_Scale.z = 1.5f;
	}
	
	// 右クリックで弾を発射する。
	if (Mouse_IsRightButtonDownTrigger() && m_ShotCoolTime <= 0.0f)
	{
		Bullet* bullet = Manager::AddGameObject<Bullet>();
		bullet->SetPosition(m_Position);
		bullet->SetVelocity(GetForward() * 5.0f);
		m_ShotCoolTime = kShotCoolTime;
	}

	// 左クリックでバットを振る。
	if (Mouse_IsLeftButtonDownTrigger() && m_Bat != nullptr)
	{
		m_Bat->StartSwing();
		m_Scale.x = 1.18f;
		m_Scale.z = 1.18f;
	}

	// 移動中の上下揺れアニメーション。
	if(GetOnGround())
	{
		m_MoveAnimation += m_Velocity.length() * dt;
		m_Scale.y = 1.0f + sinf(m_MoveAnimation*3.0f)*0.03f;
	}

	// 木との円形衝突判定。
	auto trees = Manager::GetGameObjects<Tree>();
	for (Tree* tree : trees)
	{
		Vector3 treePos = tree->GetPosition();
		Vector3 playerPos = m_Position;

		treePos.y = 0.0f;
		playerPos.y = 0.0f;

		Vector3 direction = playerPos - treePos;
		float length = direction.length();

		if (length < 1.5f)
		{
			direction /= length;;// 押し戻し方向を正規化する。
			direction *= 1.5f-length;// めり込んだ分だけ押し戻す。
			m_Position += direction;// 補正後の座標を反映する。
		}
	}
	// 箱との簡易AABB衝突判定。
	auto boxes = Manager::GetGameObjects<Box>();
	for (Box* box : boxes)
	{
		Vector3 boxPos = box->GetPosition();
		Vector3 boxScale = box->GetScale();
		
		if (boxPos.x - boxScale.x < m_Position.x &&
			m_Position.x < boxPos.x + boxScale.x &&
			boxPos.z - boxScale.z < m_Position.z &&
			m_Position.z < boxPos.z + boxScale.z)
		{
			if (boxPos.y + boxScale.y < m_Position.y &&
				m_Position.y < boxPos.y + boxScale.y * 2.0f)
			{
				// 箱の上に着地した場合。
				m_Position.y = boxPos.y + boxScale.y * 2.0f;
				m_Velocity.y = 0.0f;
				m_OnGround = true;
			}
			else if (boxPos.y - boxScale.y < m_Position.y &&
				m_Position.y < boxPos.y+boxScale.y)
			{
				// 側面に当たった場合は横移動を戻す。
				m_Position.x = oldPosition.x;
				m_Position.z = oldPosition.z;
				m_Velocity.x = 0.0f;
				m_Velocity.z = 0.0f;
			}
			
		}
	}
	GameObject::Update();
}

void Player::Draw()
{
	// 入力レイアウトを設定する。
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());

	// シェーダーを設定する。
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);


	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y + XM_PI, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}






