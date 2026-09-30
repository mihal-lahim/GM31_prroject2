#include "classroomWalk.h"

#include "animationModel.h"
#include "cloth.h"
#include "collider.h"
#include "gameSettings.h"
#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "keyboard_utf8.h"
#include "main.h"
#include "mouseInput.h"
#include "modelRenderer.h"
#include "renderer.h"
#include "sky.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
	constexpr int kTerrainHalfSize = 32;
	constexpr int kTerrainSide = kTerrainHalfSize * 2 + 1;
	constexpr float kFloatingOriginDepth = 1.10f;
	constexpr float kSwimmingChestDepth = 0.12f;

	float HeightSample(float x, float z)
	{
		auto naturalHeight = [](float sampleX, float sampleZ)
		{
			const float rolling = 0.42f * sinf(sampleX * 0.23f) * cosf(sampleZ * 0.21f)
				+ 0.18f * sinf(sampleX * 0.57f + sampleZ * 0.31f);
			// 基準ボーンと3つの丘ボーンで静的な地形をCPUスキニングする。
			const float hillWeightA = 0.66f * expf(-((sampleX + 10.0f) * (sampleX + 10.0f)
				+ (sampleZ - 13.0f) * (sampleZ - 13.0f)) / 90.0f);
			const float hillWeightB = 0.78f * expf(-((sampleX - 14.0f) * (sampleX - 14.0f)
				+ (sampleZ + 7.0f) * (sampleZ + 7.0f)) / 120.0f);
			// 開始位置から+Zへ進むと急斜面に入る。足IKの確認用。
			const float steepWeight = 0.85f * expf(-(sampleX * sampleX
				+ (sampleZ - 14.0f) * (sampleZ - 14.0f)) / 18.0f);
			const float baseWeight = 1.0f - hillWeightA - hillWeightB - steepWeight;
			return rolling * baseWeight
				+ (rolling + 2.9f) * hillWeightA
				+ (rolling + 3.2f) * hillWeightB
				+ (rolling + 5.0f) * steepWeight;
		};

		const float distance = sqrtf((x - ClassroomLake::CenterX) * (x - ClassroomLake::CenterX)
			+ (z - ClassroomLake::CenterZ) * (z - ClassroomLake::CenterZ));
		const float natural = naturalHeight(x, z);
		if (distance >= ClassroomLake::BasinRadius) return natural;

		constexpr float floorHeight = ClassroomLake::WaterLevel - 1.45f;
		constexpr float bankHeight = ClassroomLake::WaterLevel + 0.25f;
		if (distance <= ClassroomLake::FloorRadius) return floorHeight;
		if (distance < ClassroomLake::ShoreRadius)
		{
			const float edge = (distance - ClassroomLake::FloorRadius)
				/ (ClassroomLake::ShoreRadius - ClassroomLake::FloorRadius);
			const float smoothEdge = edge * edge * (3.0f - 2.0f * edge);
			return floorHeight + (bankHeight - floorHeight) * smoothEdge;
		}
		const float edge = (distance - ClassroomLake::ShoreRadius)
			/ (ClassroomLake::BasinRadius - ClassroomLake::ShoreRadius);
		const float smoothEdge = edge * edge * (3.0f - 2.0f * edge);
		return bankHeight + (natural - bankHeight) * smoothEdge;
	}

	float GridHeight(float x, float z)
	{
		static const std::array<float, kTerrainSide * kTerrainSide> heights = []
		{
			std::array<float, kTerrainSide * kTerrainSide> values{};
			for (int row = 0; row < kTerrainSide; ++row)
			{
				for (int column = 0; column < kTerrainSide; ++column)
				{
					values[row * kTerrainSide + column] = HeightSample(
						static_cast<float>(column - kTerrainHalfSize),
						static_cast<float>(kTerrainHalfSize - row));
				}
			}
			return values;
		}();
		const int column = static_cast<int>(x) + kTerrainHalfSize;
		const int row = kTerrainHalfSize - static_cast<int>(z);
		return heights[row * kTerrainSide + column];
	}
}

void ClassroomPlayer::Init()
{
	m_Position = { 0.0f, ClassroomField::GetHeight(0.0f, 0.0f), 0.0f };
	m_Model = AddComponent<AnimationModel>();
	m_Model->Load("assets/model/Akai.fbx");
	m_Model->LoadAnimation("assets/model/Akai_Idle.fbx", "Idle");
	m_Model->LoadAnimation("assets/model/Akai_Run.fbx", "Run");
	m_Model->LoadAnimation("assets/model/Jumping.fbx", "Jump");
	m_Model->LoadAnimation("assets/model/Treading Water.fbx", "TreadWater");
	m_Model->LoadAnimation("assets/model/Swimming To Edge.fbx", "Swim");
	m_Model->LoadAnimation("assets/model/Flair.fbx", "Flair");
	m_Model->SetModelScale(0.01f);
	// Akai のモデル正面は -Z。移動角度の基準 (+Z) に合わせる。
	m_Model->SetModelYawOffset(XM_PI);
	m_Model->SetFootIK(ClassroomField::GetHeight, 1.0f, ClassroomField::GetNormal);
	m_Model->PlayAnimation("Idle", 0.0f);
	m_Model->TickAnimation(0.0f, false);
	AlignFeetToField();
	m_Model->TickAnimation(0.0f);
	// のれんなどの布が押しのけられるよう、体の大きさのカプセルを付けます(足元が原点)。
	AddComponent<Collider>()
		->SetCapsule(0.24f, 1.7f)
		->SetOffset({ 0.0f, 0.85f, 0.0f })
		->SetDebugDraw(false);
}

void ClassroomPlayer::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_I))
	{
		m_ShaderPreview = !m_ShaderPreview;
		if (m_ShaderPreview) SetShaderDefinition("sample-stripes");
		else UseDefaultShader();
	}
	Vector3 input{};
	if (Keyboard_IsKeyDown(KK_A) || Keyboard_IsKeyDown(KK_LEFT)) input.x -= 1.0f;
	if (Keyboard_IsKeyDown(KK_D) || Keyboard_IsKeyDown(KK_RIGHT)) input.x += 1.0f;
	if (Keyboard_IsKeyDown(KK_W) || Keyboard_IsKeyDown(KK_UP)) input.z += 1.0f;
	if (Keyboard_IsKeyDown(KK_S) || Keyboard_IsKeyDown(KK_DOWN)) input.z -= 1.0f;

	const bool moving = input.x != 0.0f || input.z != 0.0f;
	auto inDeepWater = [](const Vector3& p)
	{
		const float dx = p.x - ClassroomLake::CenterX;
		const float dz = p.z - ClassroomLake::CenterZ;
		return dx * dx + dz * dz < 8.0f * 8.0f
			&& ClassroomField::GetHeight(p.x, p.z) < ClassroomLake::WaterLevel - 0.9f
			&& p.y < ClassroomLake::WaterLevel + 0.15f;
	};
	Vector3 position = GetPosition();
	const float groundBeforeMove = ClassroomField::GetHeight(position.x, position.z);
	const bool submergedBeforeMove = inDeepWater(position);
	if (Keyboard_IsKeyDownTrigger(KK_R) && !submergedBeforeMove
		&& position.y <= groundBeforeMove + 0.05f)
		m_Dancing = !m_Dancing;
	if (submergedBeforeMove || position.y > groundBeforeMove + 0.05f) m_Dancing = false;
	if (!submergedBeforeMove && !m_Dancing && Keyboard_IsKeyDownTrigger(KK_SPACE)
		&& position.y <= groundBeforeMove + 0.05f)
		m_VerticalSpeed = 0.18f;
	if (moving && !m_Dancing)
	{
		input.normalize();
		if (const auto* camera = GetWorld().FindFirst<ClassroomCamera>())
		{
			const float yaw = camera->GetYaw();
			const float side = input.x;
			const float forward = input.z;
			input.x = side * cosf(yaw) + forward * sinf(yaw);
			input.z = forward * cosf(yaw) - side * sinf(yaw);
		}
		const float speedPerFrame = submergedBeforeMove ? 0.045f : 0.08f; // Fixed 60 Hz step.
		Vector3 next = m_Position + input * speedPerFrame;
		next.x = std::clamp(next.x, -28.0f, 28.0f);
		next.z = std::clamp(next.z, -28.0f, 28.0f);
		SetPosition(next);
		SetRotation({ 0.0f, atan2f(input.x, input.z), 0.0f });
	}
	position = GetPosition();
	const float ground = ClassroomField::GetHeight(position.x, position.z);
	const bool inWater = inDeepWater(position);
	if (inWater)
	{
		m_Dancing = false;
		// Damp vertical motion and restore the character to a stable floating depth.
		const float floatingHeight = ClassroomWater::GetSurfaceHeight(position.x, position.z,
			GameTime::GetElapsedTime()) - kFloatingOriginDepth;
		m_VerticalSpeed += (floatingHeight - position.y) * 0.025f;
		m_VerticalSpeed *= 0.90f;
		position.y = std::clamp(position.y + m_VerticalSpeed, ground, ClassroomLake::WaterLevel);
		if (position.y == ground && m_VerticalSpeed < 0.0f) m_VerticalSpeed = 0.0f;
	}
	else
	{
		m_VerticalSpeed -= 0.009f;
		position.y += m_VerticalSpeed;
		if (position.y <= ground)
		{
			position.y = ground;
			m_VerticalSpeed = 0.0f;
		}
	}
	SetPosition(position);
	const bool airborne = !inWater && position.y > ground + 0.02f;
	const char* clip = inWater ? (moving ? "Swim" : "TreadWater")
		: airborne ? "Jump" : m_Dancing ? "Flair" : moving ? "Run" : "Idle";
	// Skip the clip's crouched anticipation: physics has already started the jump.
	// The five-second swim clip ends with an edge-climb, so loop its stroke only.
	m_Model->SetPlayback(1.0f, airborne ? 0.36f : 0.0f,
		airborne ? 0.62f : inWater && moving ? 0.18f : 1.0f, !airborne);
	const auto& previousClip = m_Model->GetAnimationName();
	const bool wasInWater = previousClip == "Swim" || previousClip == "TreadWater";
	const bool danceTransition = m_Dancing || previousClip == "Flair";
	const float blendSeconds = airborne ? 0.08f
		: wasInWater && !inWater ? 0.14f
		: inWater ? 0.22f
		: danceTransition ? 0.25f
		: previousClip == "Jump" ? 0.12f : 0.18f;
	if (previousClip != clip) m_Model->ResetFootTargetCorrection();
	m_Model->PlayAnimation(clip, blendSeconds);
	// Release terrain constraints for swimming/dance; restore them gradually on landing.
	const bool useFootIK = !inWater && !airborne && !m_Dancing;
	const float ikTarget = useFootIK ? 1.0f : 0.0f;
	m_FootIKBlend += std::clamp(ikTarget - m_FootIKBlend, -0.12f, 0.12f);
	if (inWater || airborne) m_FootIKBlend = 0.0f;
	m_Model->SetFootIK(m_FootIKBlend > 0.0f ? ClassroomField::GetHeight : nullptr,
		moving && !m_Dancing ? 0.75f : 1.0f, ClassroomField::GetNormal);
	m_Model->SetFootIKFade(m_FootIKBlend);
	m_Model->TickAnimation(1.0f / 60.0f, false);
	// The FBX's root is a standing-foot origin even in a horizontal swimming pose.
	// Align the blended chest pose with the buoyant body's waterline, not that origin.
	m_Model->SetVerticalOffset(0.0f);
	m_WaterAlignment = std::clamp(m_WaterAlignment + (inWater ? 0.08f : -0.12f), 0.0f, 1.0f);
	if (inWater)
	{
		Vector3 chest;
		if (GetSwimAnchor(chest))
		{
			const float time = GameTime::GetElapsedTime();
			const float rootSurface = ClassroomWater::GetSurfaceHeight(position.x, position.z, time);
			const float chestSurface = ClassroomWater::GetSurfaceHeight(chest.x, chest.z, time);
			const float bodyHeight = position.y + kFloatingOriginDepth;
			const float chestTarget = (std::min)(bodyHeight + chestSurface - rootSurface - kSwimmingChestDepth,
				chestSurface - 0.06f);
			m_WaterModelOffset = chestTarget - chest.y;
		}
	}
	m_Model->SetVerticalOffset(m_WaterModelOffset * m_WaterAlignment);
	if (useFootIK && m_FootIKBlend >= 0.7f) AlignFeetToField();
	m_Model->TickAnimation(0.0f);
}

void ClassroomPlayer::AlignFeetToField()
{
	// 実際にスキニングした靴底を左右別々に測り、足首のIKターゲットを更新する。
	// 走行中に持ち上げている足は接地させず、接地脚だけ補正する。
	auto solveContacts = [this]()
	{
		for (int pass = 0; pass < 6; ++pass)
		{
			float clearance[2]{};
			if (!m_Model->GetFootClearance(ClassroomField::GetHeight, clearance)) break;
			bool needsCorrection = false;
			for (int foot = 0; foot < 2; ++foot)
			{
				if (m_Model->GetFootIKWeight(foot) < 0.9f || std::abs(clearance[foot]) < 0.012f)
					continue;
				m_Model->AdjustFootTargetCorrection(foot, clearance[foot]);
				needsCorrection = true;
			}
			if (!needsCorrection) break;
			m_Model->TickAnimation(0.0f, false);
		}
	};
	solveContacts();
	float clearance[2]{};
	if (m_Model->GetFootClearance(ClassroomField::GetHeight, clearance))
	{
		float unreachableGap = 0.0f;
		for (int foot = 0; foot < 2; ++foot)
			if (m_Model->GetFootIKWeight(foot) >= 0.9f)
				unreachableGap = (std::max)(unreachableGap, clearance[foot]);
		if (unreachableGap > 0.02f)
		{
			// 脚を伸ばし切っても届かない場合だけ腰を少し下げる。
			Vector3 position = GetPosition();
			position.y -= (std::min)(unreachableGap, 0.25f);
			SetPosition(position);
			m_Model->TickAnimation(0.0f, false);
			solveContacts();
		}
	}
}

bool ClassroomPlayer::GetFootClearance(float (&clearance)[2]) const
{
	return m_Model != nullptr
		&& m_Model->GetFootClearance(ClassroomField::GetHeight, clearance);
}

bool ClassroomPlayer::GetFootEndClearance(float (&clearance)[2][2]) const
{
	return m_Model != nullptr
		&& m_Model->GetFootEndClearance(ClassroomField::GetHeight, clearance);
}

bool ClassroomPlayer::SetShaderDefinition(std::string_view id)
{
	return m_Model && m_Model->SetShaderDefinition(id);
}

void ClassroomPlayer::UseDefaultShader()
{
	if (m_Model) m_Model->UseDefaultShader();
}

std::string_view ClassroomPlayer::GetAnimationName() const
{
	return m_Model ? std::string_view(m_Model->GetAnimationName()) : std::string_view{};
}

bool ClassroomPlayer::GetSwimAnchor(Vector3& position) const
{
	return m_Model && m_Model->GetBoneWorldPosition("mixamorig:Spine2", position);
}

void ClassroomCamera::Init()
{
	m_Layer = Layer::Camera;
	m_Position = { 0.0f, 3.0f, -5.5f };
	m_Target = { 0.0f, 1.2f, 0.0f };
	m_Pitch = -atan2f(1.8f, 5.5f);
	Mouse_Reset();
}

void ClassroomCamera::PreUpdate()
{
	float mouseX = 0.0f, mouseY = 0.0f;
	if (Mouse_ConsumeDelta(&mouseX, &mouseY))
	{
		const float sensitivity = GameSettings::GetMouseSensitivity();
		m_Yaw += mouseX * sensitivity;
		m_Pitch = std::clamp(m_Pitch - mouseY * sensitivity,
			-XMConvertToRadians(65.0f), XMConvertToRadians(60.0f));
	}
	GameObject::PreUpdate();
}

void ClassroomCamera::LateUpdate()
{
	if (const auto* player = GetWorld().FindFirst<ClassroomPlayer>())
	{
		const Vector3 position = player->GetPosition();
		m_Target = position + Vector3{ 0.0f, 1.2f, 0.0f };
		constexpr float distance = 5.787054f; // sqrt(5.5^2 + 1.8^2)
		const float cosPitch = cosf(m_Pitch);
		const Vector3 forward{ sinf(m_Yaw) * cosPitch, sinf(m_Pitch), cosf(m_Yaw) * cosPitch };
		m_Position = m_Target - forward * distance;
		m_Position.y = (std::max)(m_Position.y,
			ClassroomField::GetHeight(m_Position.x, m_Position.z) + 0.35f);
	}
	m_View = XMMatrixLookAtLH(
		XMVectorSet(m_Position.x, m_Position.y, m_Position.z, 1.0f),
		XMVectorSet(m_Target.x, m_Target.y, m_Target.z, 1.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

Vector3 ClassroomCamera::GetCameraForward() const
{
	Vector3 forward = m_Target - m_Position;
	forward.normalize();
	return forward;
}

void ClassroomCamera::Draw()
{
	LIGHT light{};
	light.Enable = TRUE;
	light.Direction = { -0.45f, -0.78f, 0.43f, 0.0f };
	light.Diffuse = { 0.92f, 0.90f, 0.80f, 1.0f };
	light.Ambient = { 0.22f, 0.25f, 0.30f, 1.0f };
	light.Position = { 2.0f, 7.0f, 8.0f, 1.0f };
	light.PointLightparam = { 40.0f, 3.0f, 0.0f, 0.0f };
	light.SkyColor = { 0.50f, 0.66f, 0.82f, 1.0f };
	light.GroundColor = { 0.16f, 0.12f, 0.09f, 1.0f };
	light.GroundNormal = { 0.0f, 1.0f, 0.0f, 0.0f };
	light.Angle = { XM_PIDIV4, 0.0f, 0.0f, 0.0f };
	Renderer::SetLight(light);
	Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(1.0f,
		static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 200.0f));
	Renderer::SetViewMatrix(m_View);
	Renderer::SetCameraPosition(XMFLOAT3(m_Position.x, m_Position.y, m_Position.z));
}

float ClassroomField::GetHeight(float x, float z)
{
	// 実際に描画している2枚の三角形と同じ対角線で補間する。
	x = std::clamp(x, -static_cast<float>(kTerrainHalfSize), static_cast<float>(kTerrainHalfSize));
	z = std::clamp(z, -static_cast<float>(kTerrainHalfSize), static_cast<float>(kTerrainHalfSize));
	const float left = (std::min)(floorf(x), static_cast<float>(kTerrainHalfSize - 1));
	const float back = (std::min)(floorf(z), static_cast<float>(kTerrainHalfSize - 1));
	const float right = left + 1.0f, front = back + 1.0f;
	const float tx = x - left, tz = z - back;
	const float a = GridHeight(left, front), b = GridHeight(right, front);
	const float c = GridHeight(left, back), d = GridHeight(right, back);
	return tx <= tz
		? c * (1.0f - tz) + a * (tz - tx) + b * tx
		: c * (1.0f - tx) + d * (tx - tz) + b * tz;
}

Vector3 ClassroomField::GetNormal(float x, float z)
{
	x = std::clamp(x, -static_cast<float>(kTerrainHalfSize), static_cast<float>(kTerrainHalfSize));
	z = std::clamp(z, -static_cast<float>(kTerrainHalfSize), static_cast<float>(kTerrainHalfSize));
	const float left = (std::min)(floorf(x), static_cast<float>(kTerrainHalfSize - 1));
	const float back = (std::min)(floorf(z), static_cast<float>(kTerrainHalfSize - 1));
	const float tx = x - left, tz = z - back;
	const float a = GridHeight(left, back + 1.0f);
	const float b = GridHeight(left + 1.0f, back + 1.0f);
	const float c = GridHeight(left, back);
	const float d = GridHeight(left + 1.0f, back);
	Vector3 normal = tx <= tz
		? Vector3{ a - b, 1.0f, c - a }
		: Vector3{ c - d, 1.0f, d - b };
	normal.normalize();
	return normal;
}

void ClassroomField::Init()
{
	std::vector<VERTEX_3D> vertices;
	std::vector<unsigned int> indices;
	vertices.reserve(kTerrainSide * kTerrainSide);
	indices.reserve((kTerrainSide - 1) * (kTerrainSide - 1) * 6);
	for (int row = 0; row < kTerrainSide; ++row)
	{
		const float z = static_cast<float>(kTerrainHalfSize - row);
		for (int column = 0; column < kTerrainSide; ++column)
		{
			const float x = static_cast<float>(column - kTerrainHalfSize);
			const float y = GridHeight(x, z);
			const float slopeX = (HeightSample(x + 0.5f, z) - HeightSample(x - 0.5f, z));
			const float slopeZ = (HeightSample(x, z + 0.5f) - HeightSample(x, z - 0.5f));
			Vector3 normal{ -slopeX, 1.0f, -slopeZ };
			normal.normalize();
			const float shade = std::clamp(0.86f + y * 0.035f + normal.y * 0.1f, 0.75f, 1.0f);
			VERTEX_3D vertex{};
			vertex.Position = { x, y, z };
			vertex.Normal = { normal.x, normal.y, normal.z };
			vertex.Diffuse = { shade, shade, shade, 1.0f };
			vertex.TexCoord = { x * 0.24f, z * 0.24f };
			vertices.push_back(vertex);
		}
	}
	for (int row = 0; row < kTerrainSide - 1; ++row)
	{
		for (int column = 0; column < kTerrainSide - 1; ++column)
		{
			const unsigned int a = row * kTerrainSide + column;
			const unsigned int b = a + 1;
			const unsigned int c = a + kTerrainSide;
			const unsigned int d = c + 1;
			indices.insert(indices.end(), { a, b, c, b, d, c });
		}
	}
	D3D11_BUFFER_DESC description{};
	description.Usage = D3D11_USAGE_DEFAULT;
	description.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(VERTEX_3D));
	description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = vertices.data();
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, description, &data, "ClassroomField");
	description.ByteWidth = static_cast<UINT>(indices.size() * sizeof(unsigned int));
	description.BindFlags = D3D11_BIND_INDEX_BUFFER;
	data.pSysMem = indices.data();
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, description, &data, "ClassroomFieldIndices") && m_RenderingAvailable;
	m_IndexCount = static_cast<UINT>(indices.size());
	m_Texture = ResourceManager::LoadTexture(L"assets/model/grass.jpg");
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;
	m_RenderingAvailable = m_Surface.Load(Engine::SurfaceShader::Toon, m_Texture)
		&& m_RenderingAvailable;
	m_Surface.Parameter.x = 3.0f;
}

void ClassroomField::Draw()
{
	if (!m_RenderingAvailable) return;
	auto* context = Renderer::GetDeviceContext();
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	m_Surface.Bind();
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_IndexCount, 0, 0);
}

void ClassroomField::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_P)) SetShaderDefinition("sample-stripes");
	if (Keyboard_IsKeyDownTrigger(KK_M)) Renderer::SetPostEffectDefinition("post-invert");
	if (Keyboard_IsKeyDownTrigger(KK_O))
		if (auto* donut = GetWorld().FindFirst<ClassroomShaderDonut>()) donut->ToggleOutline();
	if (Keyboard_IsKeyDownTrigger(KK_D0))
	{
		const PostEffectMode current = Renderer::GetPostEffect();
		Renderer::SetPostEffect(current == PostEffectMode::None ? PostEffectMode::Grayscale
			: current == PostEffectMode::Grayscale ? PostEffectMode::Mosaic : PostEffectMode::None);
	}
	const struct { Keyboard_Keys key; PostEffectMode mode; } effects[] = {
		{ KK_F6, PostEffectMode::MosaicAverage },
		{ KK_F7, PostEffectMode::CircleMosaic },
		{ KK_F8, PostEffectMode::Horror },
		{ KK_F9, PostEffectMode::MipPreview },
		{ KK_F10, PostEffectMode::Shape },
		{ KK_F11, PostEffectMode::RGBShift },
		{ KK_F12, PostEffectMode::Posterize }
	};
	for (const auto& effect : effects)
		if (Keyboard_IsKeyDownTrigger(effect.key)) Renderer::SetPostEffect(effect.mode);
	const struct { Keyboard_Keys key; Engine::SurfaceShader shader; } choices[] = {
		{ KK_D1, Engine::SurfaceShader::Unlit },
		{ KK_D2, Engine::SurfaceShader::VertexDirectional },
		{ KK_D3, Engine::SurfaceShader::Directional },
		{ KK_D4, Engine::SurfaceShader::BlinnPhong },
		{ KK_D5, Engine::SurfaceShader::Hemisphere },
		{ KK_D6, Engine::SurfaceShader::Toon },
		{ KK_D7, Engine::SurfaceShader::Bump },
		{ KK_D8, Engine::SurfaceShader::CookTorrance },
		{ KK_D9, Engine::SurfaceShader::Grayscale },
		{ KK_F1, Engine::SurfaceShader::Point },
		{ KK_F2, Engine::SurfaceShader::Rim },
		{ KK_F3, Engine::SurfaceShader::Spot },
		{ KK_F4, Engine::SurfaceShader::ToonRamp },
		{ KK_F5, Engine::SurfaceShader::Disney }
	};
	for (const auto& choice : choices)
		if (Keyboard_IsKeyDownTrigger(choice.key)) SetShaderMode(choice.shader);
}

const wchar_t* ClassroomField::GetShaderName() const
{
	if (const auto* definition = m_Surface.Definition()) return definition->DisplayName;
	switch (m_Surface.Shader)
	{
	case Engine::SurfaceShader::Unlit: return L"Unlit";
	case Engine::SurfaceShader::VertexDirectional: return L"Vertex Light";
	case Engine::SurfaceShader::Directional: return L"Pixel Light";
	case Engine::SurfaceShader::BlinnPhong: return L"Blinn-Phong";
	case Engine::SurfaceShader::Hemisphere: return L"Hemisphere";
	case Engine::SurfaceShader::Toon: return L"Toon";
	case Engine::SurfaceShader::Bump: return L"Bump";
	case Engine::SurfaceShader::CookTorrance: return L"Cook-Torrance";
	case Engine::SurfaceShader::Grayscale: return L"Grayscale";
	case Engine::SurfaceShader::Point: return L"Point";
	case Engine::SurfaceShader::Rim: return L"Rim";
	case Engine::SurfaceShader::Spot: return L"Spot";
	case Engine::SurfaceShader::ToonRamp: return L"Toon Ramp";
	case Engine::SurfaceShader::Disney: return L"Disney PBR";
	default: return L"Other";
	}
}

bool ClassroomField::SetShaderMode(Engine::SurfaceShader shader)
{
	if (shader == Engine::SurfaceShader::Water) return false;
	const bool loaded = m_Surface.Load(shader, m_Texture);
	m_Surface.DetailTexture = shader == Engine::SurfaceShader::Bump
		? ResourceManager::LoadTexture(L"assets/texture/Normal.png")
		: shader == Engine::SurfaceShader::ToonRamp
		? ResourceManager::LoadTexture(L"assets/texture/Toon2.png") : TextureHandle{};
	if (shader == Engine::SurfaceShader::Toon) m_Surface.Parameter.x = 3.0f;
	if (shader == Engine::SurfaceShader::Bump) m_Surface.Parameter.x = 2.5f;
	if (shader == Engine::SurfaceShader::CookTorrance) m_Surface.Parameter.x = 0.45f;
	if (shader == Engine::SurfaceShader::ToonRamp)
		m_Surface.Parameter = { 4.0f, 0.02f, 0.65f, 0.0f };
	if (shader == Engine::SurfaceShader::Disney)
		m_Surface.Parameter = { 0.45f, 0.1f, 3.0f, 0.0f };
	if (auto* donut = GetWorld().FindFirst<ClassroomShaderDonut>()) donut->SetShaderMode(shader);
	return loaded;
}

bool ClassroomField::SetShaderDefinition(std::string_view id)
{
	if (!m_Surface.Load(id, m_Texture)) return false;
	m_Surface.DetailTexture.reset();
	if (auto* donut = GetWorld().FindFirst<ClassroomShaderDonut>())
		return donut->SetShaderDefinition(id);
	return true;
}

void ClassroomShaderDonut::Init()
{
	constexpr float x = 4.0f, z = 7.0f;
	SetPosition({ x, ClassroomLake::WaterLevel + 1.3f, z });
	SetRotation({ -XM_PIDIV4, 0.0f, 0.0f });
	SetScale({ 0.015f, 0.015f, 0.015f });
	m_Model = AddComponent<ModelRenderer>();
	m_Model->Load("assets/model/shaderDonut.obj");
	m_Model->SetUseOwnerTransform(true);
	SetShaderMode(Engine::SurfaceShader::Toon);
}

void ClassroomShaderDonut::SetShaderMode(Engine::SurfaceShader shader)
{
	m_DefinitionId.clear();
	m_Shader = shader;
	if (!m_Model) return;
	m_Model->SetShaderMode(m_Outline ? Engine::SurfaceShader::ToonOutline : shader,
		shader == Engine::SurfaceShader::ToonRamp || m_Outline
			? XMFLOAT4{ 4.0f, 0.02f, 0.65f, 0.0f }
			: XMFLOAT4{ 0.45f, 0.0f, 1.0f, 0.0f });
	if (shader == Engine::SurfaceShader::Bump)
		m_Model->SetShaderDetailTexture("assets/texture/Normal.png");
	else if (shader == Engine::SurfaceShader::ToonRamp || m_Outline)
		m_Model->SetShaderDetailTexture("assets/texture/Toon2.png");
	else
		m_Model->SetShaderDetailTexture({});
}

bool ClassroomShaderDonut::SetShaderDefinition(std::string_view id)
{
	if (!m_Model || !m_Model->SetShaderDefinition(id)) return false;
	m_DefinitionId = id;
	m_Outline = false;
	m_Model->SetShaderDetailTexture({});
	return true;
}

void ClassroomShaderDonut::ToggleOutline()
{
	m_Outline = !m_Outline;
	if (m_Outline)
	{
		m_Model->SetShaderMode(Engine::SurfaceShader::ToonOutline,
			{ 4.0f, 0.02f, 0.65f, 0.0f });
		m_Model->SetShaderDetailTexture("assets/texture/Toon2.png");
	}
	else if (!m_DefinitionId.empty())
	{
		const std::string id = m_DefinitionId;
		SetShaderDefinition(id);
	}
	else
		SetShaderMode(m_Shader);
}

void ClassroomTree::Init()
{
	m_Layer = Layer::Effect;
	SetRenderCategory(RenderCategory::Opaque);
	VERTEX_3D vertices[4]{};
	const XMFLOAT3 positions[] = {
		{ -1.7f, 4.8f, 0.0f }, { 1.7f, 4.8f, 0.0f },
		{ -1.7f, 0.0f, 0.0f }, { 1.7f, 0.0f, 0.0f }
	};
	const XMFLOAT2 uvs[] = { {0, 0}, {1, 0}, {0, 1}, {1, 1} };
	for (int i = 0; i < 4; ++i)
	{
		vertices[i].Position = positions[i];
		vertices[i].Normal = { 0.0f, 0.0f, -1.0f };
		vertices[i].Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
		vertices[i].TexCoord = uvs[i];
	}
	D3D11_BUFFER_DESC description{};
	description.Usage = D3D11_USAGE_DEFAULT;
	description.ByteWidth = sizeof(vertices);
	description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = vertices;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, description, &data, "ClassroomTree");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_Layout, m_PixelShader, "ClassroomTree") && m_RenderingAvailable;
	m_RenderingAvailable = Renderer::CreatePixelShader(m_PixelShader, "classroomTreePS.cso")
		&& m_RenderingAvailable;
	m_Texture = ResourceManager::LoadTexture(L"assets/texture/tree.png");
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;
}

void ClassroomTree::Draw()
{
	if (!m_RenderingAvailable) return;
	const ICameraView* camera = GetWorld().FindFirst<ICameraView>();
	if (camera == nullptr) return;
	auto* context = Renderer::GetDeviceContext();
	context->IASetInputLayout(m_Layout.Get());
	context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	XMMATRIX facing = XMMatrixInverse(nullptr, camera->GetViewMatrix());
	facing.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	Renderer::SetWorldMatrix(XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z)
		* facing * XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z));
	MATERIAL material{};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.Ambient = material.Diffuse;
	material.TextureEnable = true;
	Renderer::SetMaterial(material);
	ID3D11ShaderResourceView* texture = m_Texture->Get();
	context->PSSetShaderResources(0, 1, &texture);
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer::SetDepthMode(RenderDepthMode::Enabled);
	context->Draw(4, 0);
}

void ClassroomGrass::Init()
{
	m_Layer = Layer::Effect;
	SetRenderCategory(RenderCategory::Opaque);
	VERTEX_3D vertices[4]{};
	const XMFLOAT3 positions[] = {
		{ -0.65f, 1.15f, 0.0f }, { 0.65f, 1.15f, 0.0f },
		{ -0.65f, 0.0f, 0.0f }, { 0.65f, 0.0f, 0.0f }
	};
	const XMFLOAT2 uvs[] = { {0, 0}, {1, 0}, {0, 1}, {1, 1} };
	for (int i = 0; i < 4; ++i)
	{
		vertices[i].Position = positions[i];
		vertices[i].Normal = { 0.0f, 0.0f, -1.0f };
		vertices[i].Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
		vertices[i].TexCoord = uvs[i];
	}
	D3D11_BUFFER_DESC description{};
	description.Usage = D3D11_USAGE_DEFAULT;
	description.ByteWidth = sizeof(vertices);
	description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = vertices;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, description, &data, "ClassroomGrass");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_Layout, m_PixelShader, "ClassroomGrass") && m_RenderingAvailable;
	m_RenderingAvailable = Renderer::CreatePixelShader(m_PixelShader, "classroomTreePS.cso")
		&& m_RenderingAvailable;
	m_Texture = ResourceManager::LoadTexture(L"assets/texture/grass.png");
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;
}

void ClassroomGrass::Draw()
{
	if (!m_RenderingAvailable) return;
	const ICameraView* camera = GetWorld().FindFirst<ICameraView>();
	if (camera == nullptr) return;
	auto* context = Renderer::GetDeviceContext();
	context->IASetInputLayout(m_Layout.Get());
	context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	XMMATRIX facing = XMMatrixInverse(nullptr, camera->GetViewMatrix());
	facing.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	const float phase = m_Position.x * 0.71f + m_Position.z * 1.37f;
	const float time = GameTime::GetElapsedTime();
	const float sway = 0.18f * sinf(time * 2.4f + phase)
		+ 0.07f * sinf(time * 0.85f + phase * 0.43f);
	// 行ベクトルのせん断。y=0の根元は動かず、先端ほど風で横に移動する。
	const XMMATRIX bend = XMMatrixSet(
		1.0f, 0.0f, 0.0f, 0.0f,
		sway, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	Renderer::SetWorldMatrix(bend * XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z)
		* facing * XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z));
	MATERIAL material{};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.Ambient = material.Diffuse;
	material.TextureEnable = true;
	Renderer::SetMaterial(material);
	ID3D11ShaderResourceView* texture = m_Texture->Get();
	context->PSSetShaderResources(0, 1, &texture);
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer::SetDepthMode(RenderDepthMode::Enabled);
	context->Draw(4, 0);
}

void ClassroomFps::Init()
{
	DynamicText::Init();
	SetPosition({ 12.0f, 8.0f, 0.0f });
	SetFontSize(22);
	SetColor({ 1.0f, 0.95f, 0.3f, 1.0f });
	SetText(L"FPS: --");
	m_LastSample = Clock::now();
}

void ClassroomFps::Draw()
{
	++m_Frames;
	const auto now = Clock::now();
	const float seconds = std::chrono::duration<float>(now - m_LastSample).count();
	if (seconds >= 0.5f)
	{
	std::wstring label = L"FPS: " + std::to_wstring(static_cast<int>(m_Frames / seconds + 0.5f));
	if (const auto* field = GetWorld().FindFirst<ClassroomField>())
		label += L"   Ground: " + std::wstring(field->GetShaderName());
	if (Renderer::GetPostEffect() != PostEffectMode::None)
		label += L"   Post: " + std::to_wstring(static_cast<int>(Renderer::GetPostEffect()));
	else if (!Renderer::GetPostEffectDefinitionId().empty())
		label += L"   Post: Invert";
	SetText(label);
		m_Frames = 0;
		m_LastSample = now;
	}
	DynamicText::Draw();
}

namespace
{
	std::uint32_t PackRgba(float r, float g, float b)
	{
		auto channel = [](float value)
		{
			return static_cast<std::uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
		};
		return channel(r) | (channel(g) << 8) | (channel(b) << 16) | 0xFF000000u;
	}

	// 0から1へ滑らかに変わる境界です。模様の縁のギザギザを抑えます。
	float SmoothEdge(float edge0, float edge1, float value)
	{
		const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// 外側から見て時計回り(表面)になる箱を追加します。
	void AppendBox(std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
		const Vector3& center, const Vector3& half, const XMFLOAT4& color)
	{
		struct Face { Vector3 Normal, U, V; };
		// 各面で V×U が外向き法線になるよう U,V を選んでいます。
		const Face faces[] = {
			{ { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
			{ { -1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } },
			{ { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
			{ { 0, -1, 0 }, { -1, 0, 0 }, { 0, 0, 1 } },
			{ { 0, 0, 1 }, { 0, 1, 0 }, { 1, 0, 0 } },
			{ { 0, 0, -1 }, { 0, -1, 0 }, { 1, 0, 0 } },
		};
		auto extent = [&half](const Vector3& axis)
		{
			return fabsf(axis.x) * half.x + fabsf(axis.y) * half.y + fabsf(axis.z) * half.z;
		};
		for (const Face& face : faces)
		{
			const unsigned int base = static_cast<unsigned int>(vertices.size());
			const Vector3 middle = center + face.Normal * extent(face.Normal);
			const Vector3 u = face.U * extent(face.U);
			const Vector3 v = face.V * extent(face.V);
			const Vector3 corners[] = { middle - u - v, middle - u + v, middle + u + v, middle + u - v };
			for (const Vector3& corner : corners)
			{
				VERTEX_3D vertex{};
				vertex.Position = { corner.x, corner.y, corner.z };
				vertex.Normal = { face.Normal.x, face.Normal.y, face.Normal.z };
				vertex.Diffuse = color;
				vertices.push_back(vertex);
			}
			indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
		}
	}
}

void ClassroomNoren::Init()
{
	const float groundHeight = ClassroomField::GetHeight(PositionX, PositionZ);
	m_Position = { PositionX, groundHeight, PositionZ };
	BuildFrame(groundHeight);
	CreateClothTexture();

	// 幅1.2m・丈1.0mを3枚に分けた、店先によくある半のれんです。
	ClothSettings settings;
	settings.PanelCount = 3;
	settings.ColumnsPerPanel = 6;
	settings.Rows = 14;
	settings.Width = 1.2f;
	settings.Height = 1.0f;
	settings.PanelGap = 0.03f;
	settings.Mass = 0.18f;
	settings.Stiffness = 120.0f;
	m_Cloth = GetWorld().Spawn<Engine::Cloth>(settings);
	m_Cloth->SetPosition({ PositionX, groundHeight + 2.03f, PositionZ });
	m_Cloth->SetTexture(m_ClothTexture);
	// 湖から吹く弱い風で、立ち止まっていても少し揺れます。
	m_Cloth->SetWind({ 0.0f, 0.0f, -0.5f }, 0.9f);
}

void ClassroomNoren::BuildFrame(float groundHeight)
{
	std::vector<VERTEX_3D> vertices;
	std::vector<unsigned int> indices;
	const XMFLOAT4 wood{ 0.36f, 0.23f, 0.13f, 1.0f };
	const XMFLOAT4 darkWood{ 0.22f, 0.14f, 0.08f, 1.0f };
	// 柱は斜面でも浮かないよう、地面の少し下から立てます。
	for (float side : { -1.0f, 1.0f })
		AppendBox(vertices, indices, { PositionX + side * 0.72f, groundHeight + 0.98f, PositionZ },
			{ 0.045f, 1.18f, 0.045f }, wood);
	AppendBox(vertices, indices, { PositionX, groundHeight + 2.12f, PositionZ },
		{ 0.86f, 0.04f, 0.05f }, wood);
	// のれん棒。布の上端はこの高さに固定されます。
	AppendBox(vertices, indices, { PositionX, groundHeight + 2.03f, PositionZ },
		{ 0.66f, 0.012f, 0.012f }, darkWood);

	D3D11_BUFFER_DESC description{};
	description.Usage = D3D11_USAGE_IMMUTABLE;
	description.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(VERTEX_3D));
	description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = vertices.data();
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, description, &data, "ClassroomNoren");
	description.ByteWidth = static_cast<UINT>(indices.size() * sizeof(unsigned int));
	description.BindFlags = D3D11_BIND_INDEX_BUFFER;
	data.pSysMem = indices.data();
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, description, &data, "ClassroomNorenIndices") && m_RenderingAvailable;
	m_IndexCount = static_cast<UINT>(indices.size());
	m_RenderingAvailable = m_Surface.Load(Engine::SurfaceShader::Directional) && m_RenderingAvailable;
}

void ClassroomNoren::CreateClothTexture()
{
	// 藍色の地に白い輪の紋、裾に白線を入れた模様を手続き生成します。
	// テクスチャ座標は布全体で u:幅1.2m, v:丈1.0m に対応するので、紋が円になるよう補正します。
	constexpr UINT kSize = 256;
	std::vector<std::uint32_t> pixels(kSize * kSize);
	const float texel = 1.0f / kSize;
	for (UINT y = 0; y < kSize; ++y)
	{
		for (UINT x = 0; x < kSize; ++x)
		{
			const float u = (x + 0.5f) * texel;
			const float v = (y + 0.5f) * texel;
			float r = 0.11f, g = 0.17f, b = 0.36f;
			// 上端は棒を包む袋縫いなので少し暗くします。
			const float topBand = 1.0f - SmoothEdge(0.06f, 0.07f, v);
			r *= 1.0f - 0.3f * topBand; g *= 1.0f - 0.3f * topBand; b *= 1.0f - 0.3f * topBand;
			const float meterX = (u - 0.5f) * 1.2f;
			const float meterY = (v - 0.45f) * 1.0f;
			const float distance = sqrtf(meterX * meterX + meterY * meterY);
			const float ring = SmoothEdge(0.018f, 0.012f, fabsf(distance - 0.13f));
			const float dot = SmoothEdge(0.042f, 0.036f, distance);
			const float hem = SmoothEdge(0.004f, 0.0f, fabsf(v - 0.905f) - 0.006f);
			const float white = (std::max)((std::max)(ring, dot), hem);
			r += (0.93f - r) * white; g += (0.91f - g) * white; b += (0.86f - b) * white;
			pixels[y * kSize + x] = PackRgba(r, g, b);
		}
	}
	ComPtr<ID3D11ShaderResourceView> view;
	if (Engine::GraphicsResourceFactory::CreateTextureFromPixels(
		view, kSize, kSize, pixels.data(), "ClassroomNorenTexture"))
	{
		auto texture = std::make_shared<TextureResource>();
		texture->View = view;
		m_ClothTexture = texture;
	}
}

void ClassroomNoren::Draw()
{
	auto* context = Renderer::GetDeviceContext();
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	m_Surface.Bind();
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_IndexCount, 0, 0);
}

void ClassroomWalkScene::Init()
{
	World& world = GetWorld();
	world.Spawn<ClassroomCamera>();
	world.Spawn<Sky>()->SetRadius(90.0f);
	world.Spawn<ClassroomField>();
	world.Spawn<ClassroomWater>();
	world.Spawn<ClassroomShaderDonut>();
	world.Spawn<ClassroomNoren>();
	world.Spawn<ClassroomPlayer>();
	world.Spawn<ClassroomFps>();
	// 固定シード相当の配置にして、授業中も毎回同じ森を再現できるようにする。
	for (int row = 0; row < 9; ++row)
	{
		for (int column = 0; column < 8; ++column)
		{
			const int hash = (row * 73 + column * 137 + row * column * 17) % 101;
			const float x = -27.0f + column * 7.7f + static_cast<float>(hash % 7 - 3) * 0.42f;
			const float z = -24.0f + row * 6.5f + static_cast<float>(hash % 5 - 2) * 0.48f;
			if (std::abs(x) < 3.8f && z < 20.0f) continue;
			const float pondDistance = std::sqrt((x - ClassroomLake::CenterX) * (x - ClassroomLake::CenterX)
				+ (z - ClassroomLake::CenterZ) * (z - ClassroomLake::CenterZ));
			if (pondDistance < 10.0f) continue;
			ClassroomTree* tree = world.Spawn<ClassroomTree>();
			tree->SetPosition({ x, ClassroomField::GetHeight(x, z), z });
			const float size = 0.75f + static_cast<float>(hash % 6) * 0.09f;
			tree->SetScale({ size, size, size });
		}
	}
	for (int row = 0; row < 9; ++row)
	{
		for (int column = 0; column < 10; ++column)
		{
			const int hash = (row * 59 + column * 43 + row * column * 11) % 97;
			const float x = -25.0f + column * 5.5f + static_cast<float>(hash % 5 - 2) * 0.36f;
			const float z = -21.0f + row * 6.0f + static_cast<float>(hash % 7 - 3) * 0.31f;
			if (std::abs(x) < 1.6f && z > -3.0f && z < 20.0f) continue;
			const float pondDistance = std::sqrt((x - ClassroomLake::CenterX) * (x - ClassroomLake::CenterX)
				+ (z - ClassroomLake::CenterZ) * (z - ClassroomLake::CenterZ));
			if (pondDistance < 10.0f) continue;
			ClassroomGrass* grass = world.Spawn<ClassroomGrass>();
			grass->SetPosition({ x, ClassroomField::GetHeight(x, z) + 0.015f, z });
			const float size = 0.72f + static_cast<float>(hash % 6) * 0.09f;
			grass->SetScale({ size, size, size });
		}
	}
}
