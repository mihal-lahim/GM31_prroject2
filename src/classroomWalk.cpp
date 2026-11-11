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
#include "renderer.h"
#include "shadowMap.h"
#include "sky.h"
#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
	constexpr float kFloatingOriginDepth = 1.10f;
	constexpr float kSwimmingChestDepth = 0.12f;
}

void ClassroomPlayer::Init()
{
	m_Position = { 0.0f, MeshField::GetHeight(0.0f, 0.0f), 0.0f };
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
	m_Model->SetFootIK(MeshField::GetHeight, 1.0f, MeshField::GetNormal);
	m_Model->PlayAnimation("Idle", 0.0f);
	m_Model->TickAnimation(0.0f, false);
	AlignFeetToField();
	m_Model->TickAnimation(0.0f);
	UseDefaultShader();
	// のれんなどの布が押しのけられるよう、体の大きさのカプセルを付けます(足元が原点)。
	AddComponent<Collider>()
		->SetCapsule(0.24f, 1.7f)
		->SetOffset({ 0.0f, 0.85f, 0.0f })
		->SetDebugDraw(false);
}

void ClassroomPlayer::Update()
{
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
			&& MeshField::GetHeight(p.x, p.z) < ClassroomLake::WaterLevel - 0.9f
			&& p.y < ClassroomLake::WaterLevel + 0.15f;
	};
	Vector3 position = GetPosition();
	const float groundBeforeMove = MeshField::GetHeight(position.x, position.z);
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
	const float ground = MeshField::GetHeight(position.x, position.z);
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
	m_Model->SetFootIK(m_FootIKBlend > 0.0f ? MeshField::GetHeight : nullptr,
		moving && !m_Dancing ? 0.75f : 1.0f, MeshField::GetNormal);
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
			if (!m_Model->GetFootClearance(MeshField::GetHeight, clearance)) break;
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
	if (m_Model->GetFootClearance(MeshField::GetHeight, clearance))
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
		&& m_Model->GetFootClearance(MeshField::GetHeight, clearance);
}

bool ClassroomPlayer::GetFootEndClearance(float (&clearance)[2][2]) const
{
	return m_Model != nullptr
		&& m_Model->GetFootEndClearance(MeshField::GetHeight, clearance);
}

bool ClassroomPlayer::SetShaderDefinition(std::string_view id)
{
	return m_Model && m_Model->SetShaderDefinition(id);
}

void ClassroomPlayer::UseDefaultShader()
{
	// 服と革の中間くらいのつや。逆光で輪郭が少し光るようにリムを入れます。
	if (!m_Model || !m_Model->SetShaderDefinition("realistic")) return;
	m_Model->SetShaderFloat("roughness", 0.62f);
	m_Model->SetShaderFloat("metallic", 0.0f);
	m_Model->SetShaderFloat("bump", 0.0f);
	m_Model->SetShaderFloat("rim", 0.45f);
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
			MeshField::GetHeight(m_Position.x, m_Position.z) + 0.35f);
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

namespace
{
	// 木と草のビルボード共通。周りの写実表現と明るさを揃えるため、植物用の登録シェーダーを使います。
	bool LoadFoliageShaders(ComPtr<ID3D11VertexShader>& vertexShader,
		ComPtr<ID3D11InputLayout>& layout, ComPtr<ID3D11PixelShader>& pixelShader)
	{
		const auto* definition = Engine::FindShaderDefinition("realistic-foliage");
		if (definition == nullptr || definition->Passes.empty()) return false;
		const VertexShaderHandle vertex = ResourceManager::LoadVertexShader(
			std::string(definition->Passes[0].VertexFile));
		const PixelShaderHandle pixel = ResourceManager::LoadPixelShader(
			std::string(definition->Passes[0].PixelFile));
		if (!vertex || !pixel) return false;
		vertexShader = vertex->Shader;
		layout = vertex->InputLayout;
		pixelShader = pixel->Shader;
		return true;
	}
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
	m_RenderingAvailable = LoadFoliageShaders(m_VertexShader, m_Layout, m_PixelShader)
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

void ClassroomTree::DrawShadow()
{
	// 板の表(ローカル-Z)が太陽の来る方向を向くよう、水平面内で回します。
	// 太陽が高いほど影は短く、低いほど長く伸びます。
	const Vector3 light = Engine::ShadowMap::GetLightDirection();
	const float yaw = atan2f(light.x, light.z);
	auto* context = Renderer::GetDeviceContext();
	Engine::ShadowMap::BindCaster(true);
	Renderer::SetWorldMatrix(XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z)
		* XMMatrixRotationY(yaw) * XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z));
	ID3D11ShaderResourceView* texture = m_Texture->Get();
	context->PSSetShaderResources(0, 1, &texture);
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
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
	m_RenderingAvailable = LoadFoliageShaders(m_VertexShader, m_Layout, m_PixelShader)
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
		SetText(L"FPS: " + std::to_wstring(static_cast<int>(m_Frames / seconds + 0.5f)));
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
	const float groundHeight = MeshField::GetHeight(PositionX, PositionZ);
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
	// 綿の布: つやはほぼ無く、逆光で縁の毛羽が明るく見えるようリムを強めにします。
	if (m_Cloth->SetShaderDefinition("realistic"))
	{
		m_Cloth->SetShaderFloat("roughness", 0.95f);
		m_Cloth->SetShaderFloat("metallic", 0.0f);
		m_Cloth->SetShaderFloat("bump", 0.0f);
		m_Cloth->SetShaderFloat("rim", 0.8f);
	}
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
	// 塗装していない木材。やや粗く、角でわずかに光を拾う程度のつやです。
	m_RenderingAvailable = m_Surface.Load("realistic") && m_RenderingAvailable;
	m_Surface.SetFloat("roughness", 0.72f);
	m_Surface.SetFloat("metallic", 0.0f);
	m_Surface.SetFloat("bump", 0.0f);
	m_Surface.SetFloat("rim", 0.0f);
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

void ClassroomNoren::DrawShadow()
{
	auto* context = Renderer::GetDeviceContext();
	Engine::ShadowMap::BindCaster(false);
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_IndexCount, 0, 0);
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
	world.Spawn<MeshField>();
	world.Spawn<ClassroomWater>();
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
			tree->SetPosition({ x, MeshField::GetHeight(x, z), z });
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
			grass->SetPosition({ x, MeshField::GetHeight(x, z) + 0.015f, z });
			const float size = 0.72f + static_cast<float>(hash % 6) * 0.09f;
			grass->SetScale({ size, size, size });
		}
	}
}
