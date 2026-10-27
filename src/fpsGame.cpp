#include "fpsGame.h"

#include "main.h"
#include "renderer.h"
#include "collider.h"
#include "modelRenderer.h"
#include "dynamicText.h"
#include "gameSettings.h"
#include "gameTime.h"
#include "keyboard_utf8.h"
#include "mouseInput.h"
#include "sky.h"
#include "audio.h"
#include "particleSystem.h"

#include <random>

namespace
{
	constexpr float kAimSeconds = 0.2f;
	constexpr float kHipFovDegrees = 75.0f;
	constexpr float kAimFovDegrees = 60.0f;
	// 銃を固定したまま照準へ目を寄せる量。手元の相対移動にも同じ値を使います。
	const Vector3 kAimCameraOffset{ 0.17f, -0.087f, 0.12f };
	constexpr float kWalkSpeed = 5.0f;
	constexpr float kSprintSpeed = 8.0f;
	constexpr float kGravity = 20.0f;
	constexpr float kJumpSpeed = 7.0f;
	constexpr float kShotRange = 100.0f;
	// 押しっぱなしで連射する間隔(秒)と、1発ごとに視点が跳ね上がる量(ラジアン)です。
	constexpr float kFireInterval = 0.12f;
	constexpr float kViewKick = 0.012f;
	// 床に立ったときに床と接触扱いにならないよう、カプセルの下端を少し浮かせます。
	constexpr float kCapsuleLift = 0.05f;
	constexpr float kCapsuleRadius = 0.4f;
	constexpr float kCapsuleHeight = 1.8f;

	// 銃のモデル座標での銃口の先端です(scripts/generate-fps-models.js の銃口の円柱)。
	const XMFLOAT3 kMuzzleLocal{ 0.0f, 0.012f, 0.335f };
	constexpr float kMuzzleFlashSeconds = 0.05f;

	float RandomRange(float minimum, float maximum)
	{
		static std::mt19937 engine{ 31337u };
		return std::uniform_real_distribution<float>(minimum, maximum)(engine);
	}

	Vector3 RandomDirectionAround(const Vector3& normal, float spread)
	{
		Vector3 direction = normal + Vector3(RandomRange(-spread, spread),
			RandomRange(-spread, spread), RandomRange(-spread, spread));
		direction.normalize();
		return direction;
	}

	// 着弾点に火花と破片を散らします。的に当たったときは赤白の破片を多めに出します。
	void SpawnImpact(const Vector3& point, const Vector3& normal, bool hitTarget)
	{
		// 面の少し手前から出すと、面に半分埋まって見えません。
		const Vector3 origin = point + normal * 0.03f;
		const int sparkCount = hitTarget ? 14 : 9;
		for (int i = 0; i < sparkCount; ++i)
		{
			const Vector3 velocity = RandomDirectionAround(normal, 0.9f) * RandomRange(3.0f, 7.0f);
			ParticleSystem::Emit(origin, velocity, RandomRange(0.09f, 0.15f),
				static_cast<int>(RandomRange(10.0f, 20.0f)), { 1.0f, 0.78f, 0.3f, 1.0f }, -9.8f, 0.94f);
		}
		const int debrisCount = hitTarget ? 16 : 6;
		for (int i = 0; i < debrisCount; ++i)
		{
			const XMFLOAT4 color = hitTarget
				? (i % 2 == 0 ? XMFLOAT4{ 0.9f, 0.15f, 0.12f, 1.0f } : XMFLOAT4{ 0.95f, 0.95f, 0.92f, 1.0f })
				: XMFLOAT4{ 0.62f, 0.64f, 0.68f, 0.85f };
			const Vector3 velocity = RandomDirectionAround(normal, 1.2f) * RandomRange(1.0f, hitTarget ? 4.5f : 2.5f);
			ParticleSystem::Emit(origin, velocity, RandomRange(hitTarget ? 0.16f : 0.12f, hitTarget ? 0.26f : 0.2f),
				static_cast<int>(RandomRange(25.0f, 45.0f)), color, -6.0f, 0.985f);
		}
		// 着弾の瞬間に一度だけ大きく光らせます。
		ParticleSystem::Emit(origin, { 0.0f, 0.0f, 0.0f }, hitTarget ? 1.4f : 0.8f, 6,
			{ 1.0f, 0.85f, 0.5f, 1.0f }, 0.0f, 0.8f);
	}

	std::wstring FormatSeconds(float seconds)
	{
		wchar_t text[32]{};
		swprintf_s(text, L"%.2f", seconds);
		return text;
	}
}

void FpsPlayer::Init()
{
	m_Layer = Layer::Camera;
	m_Collider = AddComponent<Collider>()
		->SetCapsule(kCapsuleRadius, kCapsuleHeight)
		->SetOffset({ 0.0f, kCapsuleLift + kCapsuleHeight * 0.5f, 0.0f })
		->SetCollisionLayer(FpsCollisionLayers::Player)
		->SetCollisionMask(FpsCollisionLayers::World);
	Mouse_Reset();
}

void FpsPlayer::PreUpdate()
{
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (Mouse_ConsumeDelta(&mouseX, &mouseY))
	{
		const float sensitivity = GameSettings::GetMouseSensitivity();
		m_Yaw += mouseX * sensitivity;
		m_Pitch = std::clamp(m_Pitch - mouseY * sensitivity,
			-XMConvertToRadians(85.0f), XMConvertToRadians(85.0f));
	}
}

void FpsPlayer::Update()
{
	const float deltaTime = GameTime::GetDeltaTime();
	m_AimProgress = std::clamp(m_AimProgress
		+ (Mouse_IsRightButtonDown() ? 1.0f : -1.0f) * deltaTime / kAimSeconds, 0.0f, 1.0f);
	const Vector3 previous = m_Position;
	Move(deltaTime);
	const Vector3 moved = m_Position - previous;
	m_MoveSpeed = deltaTime > 0.0f ? Vector3(moved.x, 0.0f, moved.z).length() / deltaTime : 0.0f;

	m_FireCooldown = (std::max)(0.0f, m_FireCooldown - deltaTime);
	if (Mouse_IsLeftButtonDown() && m_FireCooldown <= 0.0f)
	{
		m_FireCooldown = kFireInterval;
		Shoot();
	}
}

void FpsPlayer::Move(float deltaTime)
{
	const Vector3 forward{ sinf(m_Yaw), 0.0f, cosf(m_Yaw) };
	const Vector3 right{ cosf(m_Yaw), 0.0f, -sinf(m_Yaw) };
	Vector3 input{ 0.0f, 0.0f, 0.0f };
	if (Keyboard_IsKeyDown(KK_W)) input += forward;
	if (Keyboard_IsKeyDown(KK_S)) input -= forward;
	if (Keyboard_IsKeyDown(KK_D)) input += right;
	if (Keyboard_IsKeyDown(KK_A)) input -= right;
	input.normalize();

	const float speed = Keyboard_IsKeyDown(KK_LEFTSHIFT) ? kSprintSpeed : kWalkSpeed;
	const Vector3 horizontal = input * (speed * deltaTime);
	// 軸ごとに動かすと、壁に斜めに当たっても壁沿いに滑ります。
	TryMoveHorizontal({ horizontal.x, 0.0f, 0.0f });
	TryMoveHorizontal({ 0.0f, 0.0f, horizontal.z });

	if (m_Grounded && Keyboard_IsKeyDownTrigger(KK_SPACE))
	{
		m_VerticalSpeed = kJumpSpeed;
		m_Grounded = false;
	}
	m_VerticalSpeed -= kGravity * deltaTime;
	const float verticalMove = m_VerticalSpeed * deltaTime;

	if (verticalMove <= 0.0f)
	{
		// 腰の高さから下へレイを飛ばし、今回の落下量の範囲に床があれば上に立たせます。
		constexpr float kProbeHeight = 0.5f;
		const Vector3 probeOrigin = m_Position + Vector3(0.0f, kProbeHeight, 0.0f);
		RaycastHit hit;
		if (GetWorld().GetCollisionWorld().Raycast(probeOrigin, { 0.0f, -1.0f, 0.0f },
			kProbeHeight - verticalMove + 0.05f, hit, FpsCollisionLayers::World, m_Collider))
		{
			SetPosition({ m_Position.x, hit.Point.y, m_Position.z });
			m_VerticalSpeed = 0.0f;
			m_Grounded = true;
			return;
		}
		m_Grounded = false;
		SetPosition(m_Position + Vector3(0.0f, verticalMove, 0.0f));
	}
	else
	{
		const Vector3 previous = m_Position;
		SetPosition(m_Position + Vector3(0.0f, verticalMove, 0.0f));
		if (!m_Collider->GetOverlaps(FpsCollisionLayers::World).empty())
		{
			// 天井に頭をぶつけたので上昇をやめます。
			SetPosition(previous);
			m_VerticalSpeed = 0.0f;
		}
	}

	// 落下しすぎたらスタート地点へ戻します。
	if (m_Position.y < -30.0f)
	{
		SetPosition({ 0.0f, 0.0f, -12.0f });
		m_VerticalSpeed = 0.0f;
	}
}

bool FpsPlayer::TryMoveHorizontal(const Vector3& offset)
{
	if (offset.x == 0.0f && offset.z == 0.0f)
	{
		return false;
	}
	const Vector3 previous = m_Position;
	SetPosition(m_Position + offset);
	if (m_Collider->GetOverlaps(FpsCollisionLayers::World).empty())
	{
		return true;
	}
	SetPosition(previous);
	return false;
}

void FpsPlayer::Shoot()
{
	if (FpsWeapon* weapon = GetWorld().FindFirst<FpsWeapon>())
	{
		weapon->Kick();
	}
	m_Pitch = (std::min)(m_Pitch + kViewKick, XMConvertToRadians(85.0f));

	RaycastHit hit;
	if (!GetWorld().GetCollisionWorld().Raycast(GetCameraPosition(), GetCameraForward(), kShotRange, hit,
		FpsCollisionLayers::World | FpsCollisionLayers::Target, m_Collider))
	{
		return;
	}
	const bool hitTarget = (hit.HitCollider->GetCollisionLayer() & FpsCollisionLayers::Target) != 0;
	SpawnImpact(hit.Point, hit.Normal, hitTarget);
	if (hitTarget)
	{
		hit.HitCollider->GetOwner()->MarkForDestroy();
		m_Score++;
		if (FpsWeapon* weapon = GetWorld().FindFirst<FpsWeapon>())
		{
			weapon->PlayTargetBreakSound();
		}
	}
}

Vector3 FpsPlayer::GetCameraPosition() const
{
	// 照準へカメラ自体を寄せます。射撃レイもこの視点から出します。
	const Vector3 right{ cosf(m_Yaw), 0.0f, -sinf(m_Yaw) };
	const Vector3 forward = GetCameraForward();
	const Vector3 up = Vector3::cross(forward, right);
	return m_Position + Vector3(0.0f, kEyeHeight, 0.0f)
		+ (right * kAimCameraOffset.x + up * kAimCameraOffset.y
			+ forward * kAimCameraOffset.z) * GetAimBlend();
}

Vector3 FpsPlayer::GetCameraForward() const
{
	const float cosPitch = cosf(m_Pitch);
	return { sinf(m_Yaw) * cosPitch, sinf(m_Pitch), cosf(m_Yaw) * cosPitch };
}

void FpsPlayer::LateUpdate()
{
	const Vector3 eye = GetCameraPosition();
	const Vector3 target = eye + GetCameraForward();
	m_View = XMMatrixLookAtLH(
		XMVectorSet(eye.x, eye.y, eye.z, 1.0f),
		XMVectorSet(target.x, target.y, target.z, 1.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

void FpsPlayer::Draw()
{
	LIGHT light{};
	light.Enable = TRUE;
	light.Direction = { -0.45f, -0.78f, 0.43f, 0.0f };
	light.Diffuse = { 0.92f, 0.90f, 0.80f, 1.0f };
	light.Ambient = { 0.30f, 0.32f, 0.36f, 1.0f };
	light.SkyColor = { 0.50f, 0.66f, 0.82f, 1.0f };
	light.GroundColor = { 0.16f, 0.12f, 0.09f, 1.0f };
	light.GroundNormal = { 0.0f, 1.0f, 0.0f, 0.0f };
	Renderer::SetLight(light);
	const float fov = kHipFovDegrees + (kAimFovDegrees - kHipFovDegrees) * GetAimBlend();
	Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(XMConvertToRadians(fov),
		static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.05f, 200.0f));
	Renderer::SetViewMatrix(m_View);
	const Vector3 eye = GetCameraPosition();
	Renderer::SetCameraPosition(XMFLOAT3(eye.x, eye.y, eye.z));
}

void FpsWeapon::Init()
{
	// Effect層のOverlayはワールドと半透明エフェクトの後、UIの前に描かれます。
	m_Layer = Layer::Effect;
	SetRenderCategory(RenderCategory::Overlay);
	m_Model = AddComponent<ModelRenderer>();
	m_Model->Load("assets\\model\\fpsGun.obj");
	if (m_Model->SetShaderDefinition("realistic"))
	{
		m_Model->SetShaderFloat("roughness", 0.38f);
		m_Model->SetShaderFloat("metallic", 0.1f);
		m_Model->SetShaderFloat("rim", 0.25f);
	}

	// 連射すると前の音が鳴り終わる前に次を撃つので、声を複数持って順番に鳴らします。
	for (Audio*& voice : m_ShotVoices)
	{
		voice = AddComponent<Audio>();
		voice->Load("assets\\audio\\fpsRifle.wav");
	}
	m_TargetBreakVoice = AddComponent<Audio>();
	m_TargetBreakVoice->Load("assets\\audio\\fpsTargetBreak.wav");
}

void FpsWeapon::Kick()
{
	m_Recoil = (std::min)(m_Recoil + 1.0f, 1.6f);
	m_FlashTime = kMuzzleFlashSeconds;
	m_FlashRoll = RandomRange(0.0f, XM_2PI);
	m_FlashScale = RandomRange(0.8f, 1.25f);
	m_ShotVoices[m_NextShotVoice]->Play();
	m_NextShotVoice = (m_NextShotVoice + 1) % kShotVoices;
}

void FpsWeapon::PlayTargetBreakSound()
{
	m_TargetBreakVoice->Play();
}

bool FpsWeapon::GetMuzzleFlash(XMMATRIX& world) const
{
	if (m_FlashTime <= 0.0f)
	{
		return false;
	}
	world = XMMatrixScaling(m_FlashScale, m_FlashScale, m_FlashScale)
		* XMMatrixRotationZ(m_FlashRoll)
		* XMMatrixTranslation(kMuzzleLocal.x, kMuzzleLocal.y, kMuzzleLocal.z)
		* m_GunWorld;
	return true;
}

void FpsWeapon::Update()
{
	const float deltaTime = GameTime::GetDeltaTime();
	// 反動は指数的に戻します。
	m_Recoil *= expf(-14.0f * deltaTime);
	m_FlashTime -= deltaTime;

	const FpsPlayer* player = GetWorld().FindFirst<FpsPlayer>();
	const bool walking = player != nullptr && player->IsGrounded() && player->GetMoveSpeed() > 0.5f;
	const float targetWeight = walking ? (std::min)(player->GetMoveSpeed() / kSprintSpeed, 1.0f) : 0.0f;
	m_BobWeight += (targetWeight - m_BobWeight) * (std::min)(1.0f, 10.0f * deltaTime);
	m_BobTime += deltaTime * (6.0f + 6.0f * m_BobWeight);
}

void FpsWeapon::Draw()
{
	FpsPlayer* player = GetWorld().FindFirst<FpsPlayer>();
	m_Model->SetVisible(player != nullptr);
	if (player == nullptr)
	{
		return;
	}

	Renderer::ClearDepth();
	// 手元だけ視野角を狭めると、画面端で銃が引き伸ばされません。
	// この後に描かれるのは2DのUIだけなので、投影行列は戻しません(次のフレームはカメラが設定します)。
	Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f),
		static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.01f, 10.0f));

	// カメラ座標での位置です。右下に構え、歩くと8の字に揺れ、撃つと後ろへ下がって銃口が上がります。
	const float aim = player->GetAimBlend();
	const float sway = 1.0f - 0.9f * aim;
	const float bobX = sinf(m_BobTime) * 0.012f * m_BobWeight * sway;
	const float bobY = -fabsf(cosf(m_BobTime)) * 0.010f * m_BobWeight * sway;
	const XMMATRIX local =
		XMMatrixRotationRollPitchYaw(-0.12f * m_Recoil, XMConvertToRadians(-2.5f * (1.0f - aim)), 0.0f)
		* XMMatrixTranslation(0.17f - kAimCameraOffset.x * aim + bobX,
			-0.17f - kAimCameraOffset.y * aim + bobY,
			0.46f - kAimCameraOffset.z * aim - 0.045f * m_Recoil);
	// ライトはワールド座標で計算されるので、カメラの逆行列でワールドへ戻してから描きます。
	m_GunWorld = local * XMMatrixInverse(nullptr, player->GetViewMatrix());
	Renderer::SetWorldMatrix(m_GunWorld);
}

void FpsMuzzleFlash::Init()
{
	// 銃より後に生成するので、同じEffect層のOverlayの中で銃の後に描かれます。
	m_Layer = Layer::Effect;
	SetRenderCategory(RenderCategory::Overlay);
	m_Model = AddComponent<ModelRenderer>();
	m_Model->Load("assets\\model\\fpsMuzzleFlash.obj");
	m_Model->SetVisible(false);
}

void FpsMuzzleFlash::Draw()
{
	const FpsWeapon* weapon = GetWorld().FindFirst<FpsWeapon>();
	XMMATRIX world = XMMatrixIdentity();
	const bool visible = weapon != nullptr && weapon->GetMuzzleFlash(world);
	m_Model->SetVisible(visible);
	if (!visible)
	{
		return;
	}
	// 加算合成で光らせ、深度は書かずに銃の手前だけへ重ねます。
	// 投影行列と深度は銃が設定した手元用のものをそのまま使います。描画状態は描画後にRenderQueueが戻します。
	Renderer::SetBlendMode(RenderBlendMode::Additive);
	Renderer::SetDepthMode(RenderDepthMode::TestOnly);
	Renderer::SetWorldMatrix(world);
}

void FpsBlock::Init()
{
	m_Layer = Layer::World;
	// fpsBlock.objは-1〜1の立方体なので、scaleがそのまま見た目と当たり判定の半サイズになります。
	ModelRenderer* model = AddComponent<ModelRenderer>()->SetUseOwnerTransform();
	model->Load("assets\\model\\fpsBlock.obj");
	// 既定のシェーダーは光を計算しないので、色だけの面でも陰影が出る写実シェーダーを使います。
	if (model->SetShaderDefinition("realistic"))
	{
		model->SetShaderFloat("roughness", 0.85f);
	}
	AddComponent<Collider>()
		->SetAABB({ 1.0f, 1.0f, 1.0f })
		->SetCollisionLayer(FpsCollisionLayers::World)
		->SetStatic();
}

void FpsTarget::Init()
{
	m_Layer = Layer::World;
	ModelRenderer* model = AddComponent<ModelRenderer>()->SetUseOwnerTransform();
	model->Load("assets\\model\\fpsTarget.obj");
	if (model->SetShaderDefinition("realistic"))
	{
		model->SetShaderFloat("roughness", 0.5f);
	}
	// 標的板の半径(0.45m)に合わせた球で当たりを取ります。
	AddComponent<Collider>()
		->SetSphere(0.45f)
		->SetCollisionLayer(FpsCollisionLayers::Target);
}

void FpsTarget::Update()
{
	m_Rotation.y += 1.5f * GameTime::GetDeltaTime();
}

void FpsGameScene::Init()
{
	Collider::SetDebugDrawEnabled(false);
	World& world = GetWorld();

	FpsPlayer* player = world.Spawn<FpsPlayer>();
	player->SetPosition({ 0.0f, 0.0f, -12.0f });
	world.Spawn<Sky>()->SetRadius(90.0f);
	world.Spawn<ParticleSystem>();
	world.Spawn<FpsWeapon>();
	world.Spawn<FpsMuzzleFlash>();

	const auto block = [&world](const Vector3& center, const Vector3& halfSize)
	{
		FpsBlock* spawned = world.Spawn<FpsBlock>();
		spawned->SetPosition(center);
		spawned->SetScale(halfSize);
	};
	// 床と外周の壁です。
	block({ 0.0f, -0.5f, 0.0f }, { 20.0f, 0.5f, 20.0f });
	block({ 0.0f, 1.5f, 20.5f }, { 20.0f, 1.5f, 0.5f });
	block({ 0.0f, 1.5f, -20.5f }, { 20.0f, 1.5f, 0.5f });
	block({ 20.5f, 1.5f, 0.0f }, { 0.5f, 1.5f, 20.0f });
	block({ -20.5f, 1.5f, 0.0f }, { 0.5f, 1.5f, 20.0f });
	// 遮蔽物と足場です。
	block({ -6.0f, 0.75f, 0.0f }, { 1.5f, 0.75f, 1.5f });
	block({ 5.0f, 1.0f, 4.0f }, { 2.0f, 1.0f, 1.0f });
	block({ 0.0f, 0.5f, 10.0f }, { 4.0f, 0.5f, 1.0f });
	block({ 10.0f, 1.5f, -6.0f }, { 1.0f, 1.5f, 3.0f });

	const Vector3 targetPositions[] =
	{
		{ -6.0f, 2.2f, 0.0f },
		{ 5.0f, 2.7f, 4.0f },
		{ 0.0f, 1.7f, 10.0f },
		{ 10.0f, 3.7f, -6.0f },
		{ -12.0f, 1.5f, 12.0f },
		{ 14.0f, 1.5f, 14.0f },
	};
	for (const Vector3& position : targetPositions)
	{
		world.Spawn<FpsTarget>()->SetPosition(position);
	}

	DynamicText* crosshair = world.Spawn<DynamicText>();
	crosshair->SetAlign(DynamicText::Align::Center);
	crosshair->SetFontSize(28);
	crosshair->SetText(L"+");
	crosshair->SetPosition({ SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f - 16.0f, 0.0f });

	m_Status = world.Spawn<DynamicText>();
	m_Status->SetFontSize(24);
	m_Status->SetPosition({ 16.0f, 12.0f, 0.0f });
	m_ElapsedSeconds = 0.0f;
	m_Cleared = false;
}

void FpsGameScene::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_R))
	{
		ChangeScene<FpsGameScene>();
		return;
	}

	World& world = GetWorld();
	const FpsPlayer* player = world.FindFirst<FpsPlayer>();
	const std::size_t remaining = world.FindAll<FpsTarget>().size();
	if (!m_Cleared)
	{
		m_ElapsedSeconds += GameTime::GetDeltaTime();
		m_Cleared = remaining == 0;
	}

	std::wstring text = L"的 残り " + std::to_wstring(remaining)
		+ L"   命中 " + std::to_wstring(player != nullptr ? player->GetScore() : 0)
		+ L"   タイム " + FormatSeconds(m_ElapsedSeconds);
	if (m_Cleared)
	{
		text += L"   クリア!  R:もう一度";
	}
	m_Status->SetText(text);
}
