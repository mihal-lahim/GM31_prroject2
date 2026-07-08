

#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "keyboard.h"
#include "player.h"
#include "bullet.h"
#include "manager.h"
#include "gameTime.h"
#include "camera.h"
#include "tree.h"
#include "box.h"
#include "audio.h"
#include "playerMoveParticle.h"

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


void Player::Init()
{
	m_Layer = Layer::World;
	m_Position = { 0.0f, 0.0f, 0.0f };

	AddComponent<ModelRenderer>(this)->Load("assets\\model\\player.obj");
	
	//繧ｷ繧ｧ繝ｼ繝繝ｼ隱ｭ縺ｿ霎ｼ縺ｿ
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(&m_PixelShader, "unlitTexturePS.cso");
	//SE隱ｭ縺ｿ霎ｼ縺ｿ
	m_JumpSE = AddComponent<Audio>(this);
	m_JumpSE->Load("assets/audio/wan.wav");
	AddComponent<PlayerMoveParticle>(this);
}

void Player::Uninit()
{
	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	GameObject::Uninit();
}

void Player::Update()
{
	Vector3 oldPosition = m_Position;//蜑阪・繝輔Ξ繝ｼ繝縺ｮ菴咲ｽｮ繧剃ｿ晏ｭ倥☆繧九◆繧・

	float dt = GameTime::GetDeltaTime();//1/60遘偵↓蜍輔￥謨ｰ蛟､繧呈欠螳壹☆繧九◆繧・
	
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

	//繧ｸ繝｣繝ｳ繝・
	if (Keyboard_IsKeyDownTrigger(KK_SPACE)&&GetOnGround())
	{
		m_Velocity.y +=20.0f;//豼蜉・

		m_Scale.y = 2.0f;//繧ｸ繝｣繝ｳ繝嶺ｸｭ縺ｯ
		m_Scale.x = 0.5f;
		m_Scale.z = 0.5f;
		m_JumpSE->Play();
	}

	//繧ｹ繧ｱ繝ｼ繝ｫ繧呈綾縺・
	m_Scale.x += (1.0f - m_Scale.x) * 0.07f;
	m_Scale.y += (1.0f - m_Scale.y) * 0.07f;
	m_Scale.z += (1.0f - m_Scale.z) * 0.07f;

	//驥榊鴨驕ｩ逕ｨ
	m_Velocity.y += -50.0f * dt;
	//謚ｵ謚怜鴨
	m_Velocity.x += -m_Velocity.x * 5.0f * dt;
	m_Velocity.z += -m_Velocity.z * 5.0f * dt;

	//遘ｻ蜍募､驕ｩ逕ｨ
	m_Position += m_Velocity * dt;

	bool oldOnGround = GetOnGround();

	//陦晉ｪ∝愛螳・
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

	//縺､縺ｶ縺吶い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ
	if (!oldOnGround && GetOnGround())
	{
		m_Scale.y = 0.8f;
		m_Scale.x = 1.5f;
		m_Scale.z = 1.5f;
	}
	
	//蠑ｾ逋ｺ蟆・
	if (Keyboard_IsKeyDownTrigger(KK_J))
	{
		Bullet* bullet = Manager::AddGameObject<Bullet>();
		bullet->SetPosition(m_Position);
		bullet->SetVelocity(GetForward() * 5.0f);
	}

	//遘ｻ蜍輔い繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ
	if(GetOnGround())
	{
		m_MoveAnimation += m_Velocity.length() * dt;
		m_Scale.y = 1.0f + sinf(m_MoveAnimation*3.0f)*0.03f;
	}

	//譛ｨ縺ｨ縺ｮ陦晉ｪ∝愛螳・
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
			direction /= length;;//豁｣隕丞喧
			direction *= 1.5f-length;//驥阪↑繧雁・縺縺第款縺怜・縺・
			m_Position += direction;//繝励Ξ繧､繝､繝ｼ縺ｮ菴咲ｽｮ繧剃ｿｮ豁｣
		}
	}
	//繝懊ャ繧ｯ繧ｹ縺ｨ縺ｮ陦晉ｪ∝愛螳・
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
				//荳企擇
				m_Position.y = boxPos.y + boxScale.y * 2.0f;
				m_Velocity.y = 0.0f;
				m_OnGround = true;
			}
			else if (boxPos.y - boxScale.y < m_Position.y &&
				m_Position.y < boxPos.y+boxScale.y)
			{
				//蛛ｴ髱｢
				m_Position.x = oldPosition.x;
				m_Position.z = oldPosition.z;
				m_Velocity.x = 0.0f;
				m_Velocity.z = 0.0f;
			}
			
		}
	}
	//邯呎価蜈・・Update繧貞他縺ｳ蜃ｺ縺・
	GameObject::Update();
}

void Player::Draw()
{
	//蜈･蜉帙Ξ繧､繧｢繧ｦ繝郁ｨｭ螳・
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//繧ｷ繧ｧ繝ｼ繝繝ｼ險ｭ螳・
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);


	//繝槭ヨ繝ｪ繧ｯ繧ｹ險ｭ螳・
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y + XM_PI, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	//邯呎価蜈・・Draw繧貞他縺ｳ蜃ｺ縺・
	GameObject::Draw();
}






