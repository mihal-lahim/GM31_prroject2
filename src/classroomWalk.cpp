#include "classroomWalk.h"

#include "animationModel.h"
#include "gameSettings.h"
#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "keyboard_utf8.h"
#include "main.h"
#include "mouseInput.h"
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
	m_Model->SetModelScale(0.01f);
	// Akai のモデル正面は -Z。移動角度の基準 (+Z) に合わせる。
	m_Model->SetModelYawOffset(XM_PI);
	m_Model->SetFootIK(ClassroomField::GetHeight, 1.0f, ClassroomField::GetNormal);
	m_Model->PlayAnimation("Idle", 0.0f);
	m_Model->TickAnimation(0.0f, false);
	AlignFeetToField();
	m_Model->TickAnimation(0.0f);
}

void ClassroomPlayer::Update()
{
	Vector3 input{};
	if (Keyboard_IsKeyDown(KK_A) || Keyboard_IsKeyDown(KK_LEFT)) input.x -= 1.0f;
	if (Keyboard_IsKeyDown(KK_D) || Keyboard_IsKeyDown(KK_RIGHT)) input.x += 1.0f;
	if (Keyboard_IsKeyDown(KK_W) || Keyboard_IsKeyDown(KK_UP)) input.z += 1.0f;
	if (Keyboard_IsKeyDown(KK_S) || Keyboard_IsKeyDown(KK_DOWN)) input.z -= 1.0f;

	const bool moving = input.x != 0.0f || input.z != 0.0f;
	if (moving)
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
		constexpr float speedPerFrame = 0.08f; // Engine runs a fixed 60 Hz step.
		Vector3 next = m_Position + input * speedPerFrame;
		next.x = std::clamp(next.x, -28.0f, 28.0f);
		next.z = std::clamp(next.z, -28.0f, 28.0f);
		SetPosition(next);
		SetRotation({ 0.0f, atan2f(input.x, input.z), 0.0f });
	}
	Vector3 grounded = GetPosition();
	grounded.y = ClassroomField::GetHeight(grounded.x, grounded.z);
	SetPosition(grounded);
	m_Model->PlayAnimation(moving ? "Run" : "Idle", 0.2f);
	m_Model->SetFootIK(ClassroomField::GetHeight, moving ? 0.75f : 1.0f,
		ClassroomField::GetNormal);
	m_Model->TickAnimation(1.0f / 60.0f, false);
	AlignFeetToField();
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
	Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(1.0f,
		static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 200.0f));
	Renderer::SetViewMatrix(m_View);
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
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_Layout, m_PixelShader, "ClassroomField") && m_RenderingAvailable;
	m_Texture = ResourceManager::LoadTexture(L"assets/model/grass.jpg");
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;
}

void ClassroomField::Draw()
{
	if (!m_RenderingAvailable) return;
	auto* context = Renderer::GetDeviceContext();
	context->IASetInputLayout(m_Layout.Get());
	context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	MATERIAL material{};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.Ambient = material.Diffuse;
	material.TextureEnable = true;
	Renderer::SetMaterial(material);
	ID3D11ShaderResourceView* texture = m_Texture->Get();
	context->PSSetShaderResources(0, 1, &texture);
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_IndexCount, 0, 0);
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
		SetText(L"FPS: " + std::to_wstring(static_cast<int>(m_Frames / seconds + 0.5f)));
		m_Frames = 0;
		m_LastSample = now;
	}
	DynamicText::Draw();
}

void ClassroomWalkScene::Init()
{
	World& world = GetWorld();
	world.Spawn<ClassroomCamera>();
	world.Spawn<Sky>()->SetRadius(90.0f);
	world.Spawn<ClassroomField>();
	world.Spawn<ClassroomWater>();
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
