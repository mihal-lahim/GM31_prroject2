#include "shaderGalleryScene.h"
#include "shaderGalleryModes.h"
#include "modelRenderer.h"
#include "cameraView.h"
#include "keyboard_utf8.h"
#include "renderer.h"
#include "shaderGalleryUi.h"
#include "mouseInput.h"
#include "third_party/imgui/imgui.h"
#include <cmath>
#include <algorithm>
#include <string>

namespace
{
class GalleryCamera final : public GameObject, public ICameraView
{
	float yaw = 0.0f;
	float distance = 23.0f;
	float savedYaw = 0.0f;
	float savedDistance = 23.0f;
	Vector3 savedTarget{};
	bool wasPlaying = false;
	Vector3 position{};
	Vector3 target{};
	XMMATRIX view = XMMatrixIdentity();
public:
	void Init() override { m_Layer = Layer::Camera; }
	void PreUpdate() override
	{
		const auto& state = GetGalleryUiState();
		if (state.Playing)
		{
			if (!wasPlaying)
			{
				savedYaw = yaw; savedDistance = distance; savedTarget = target;
				wasPlaying = true;
				target = {};
			}
			const float seconds = state.PreviewSeconds;
			const float approach = (std::min)(seconds / 8.0f, 1.0f);
			const float reveal = (std::max)(0.0f, (std::min)((seconds - 18.0f) / 4.0f, 1.0f));
			yaw = -0.38f + 0.84f * approach - 0.3f * reveal;
			distance = 8.0f - 2.0f * approach + 2.0f * reveal;
			return;
		}
		if (wasPlaying)
		{
			yaw = savedYaw; distance = savedDistance; target = savedTarget;
			wasPlaying = false;
		}
		auto& mutableState = GetGalleryUiState();
		distance = (std::clamp)(distance - mutableState.WheelZoom * 1.2f, 8.0f, 35.0f);
		mutableState.WheelZoom = 0.0f;
		float dx = 0.0f, dy = 0.0f;
		if (Mouse_ConsumeDelta(&dx, &dy) && !ImGui::GetIO().WantCaptureMouse)
		{
			if (Mouse_IsLeftButtonDown()) yaw += dx * 0.004f;
			if (Mouse_IsRightButtonDown())
			{
				target.x -= dx * 0.012f;
				target.z += dy * 0.012f;
			}
		}
		if (Keyboard_IsKeyDown(KK_A)) yaw -= 0.018f;
		if (Keyboard_IsKeyDown(KK_D)) yaw += 0.018f;
		if (Keyboard_IsKeyDown(KK_W)) distance = (std::max)(8.0f, distance - 0.15f);
		if (Keyboard_IsKeyDown(KK_S)) distance = (std::min)(35.0f, distance + 0.15f);
	}
	void LateUpdate() override
	{
		position = { std::sin(yaw) * distance, 12.0f, -std::cos(yaw) * distance };
		position = position + target;
		view = XMMatrixLookAtLH(XMVectorSet(position.x, position.y, position.z, 1.0f),
			XMVectorSet(target.x, target.y, target.z, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		GetGalleryUiState().View = view;
	}
	void Draw() override
	{
		Renderer::SetLight(GetGalleryUiState().Light);
		Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(1.0f,
			static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 200.0f));
		Renderer::SetViewMatrix(view);
		Renderer::SetCameraPosition({ position.x, position.y, position.z });
	}
	XMMATRIX GetViewMatrix() const override { return view; }
	Vector3 GetCameraPosition() const override { return position; }
	Vector3 GetCameraForward() const override { Vector3 forward = Vector3{} - position; forward.normalize(); return forward; }
};

class GalleryModel final : public GameObject
{
	ModelRenderer* model = nullptr;
	int index = 0;
public:
	explicit GalleryModel(int value) : index(value) {}
	void Init() override
	{
		const int column = index % 4;
		const int row = index / 4;
		SetPosition({ (column - 1.5f) * 3.8f, 0.0f, (row - 1.5f) * 3.8f });
		SetRotation({ -XM_PIDIV4, 0.0f, 0.0f });
		SetScale({ 0.018f, 0.018f, 0.018f });
		model = AddComponent<ModelRenderer>();
		model->Load("assets/model/shaderDonut.obj");
		model->SetUseOwnerTransform(true);
		model->SetShaderMode(Engine::ShaderGalleryModes[index].Shader);
		if (index == 14) model->SetShaderDefinition("sample-stripes");
		if (index == 11 || index == 12) model->SetShaderDetailTexture("assets/texture/Toon2.png");
		if (index == 13) model->SetShaderDetailTexture("assets/texture/Normal.png");
	}
	void Update() override
	{
		const auto& state = GetGalleryUiState();
		bool visible = state.Enabled[index];
		if (state.Playing)
		{
			const float t = state.PreviewSeconds;
			visible = t < 8.0f ? index == 7 : t < 15.0f ? index == 6
				: t < 18.0f ? index == 10 : index >= 10 && index <= 12;
			SetPosition({ index >= 10 && index <= 12 ? (index - 11) * 2.5f : 0.0f, 0.0f, 0.0f });
		}
		else
		{
			const int column = index % 4, row = index / 4;
			SetPosition({ (column - 1.5f) * 3.8f, 0.0f, (row - 1.5f) * 3.8f });
		}
		model->SetVisible(visible);
		model->SetShaderParameter(state.Parameters[index]);
		if (state.Animate || state.Playing)
		{
			auto r = GetRotation(); r.y += 0.005f;
			SetRotation(r);
		}
	}
};

class GalleryFloor final : public GameObject
{
	ModelRenderer* model = nullptr;
	bool bump = false;
public:
	void Init() override
	{
		SetPosition({ 0.0f, -1.4f, 0.0f });
		SetScale({ 8.0f, 0.08f, 8.0f });
		model = AddComponent<ModelRenderer>();
		model->Load("assets/model/box.obj");
		model->SetUseOwnerTransform(true);
		model->SetShaderMode(Engine::SurfaceShader::Directional);
	}
	void Update() override
	{
		const auto& state = GetGalleryUiState();
		model->SetVisible(state.ShowFloor);
		if (bump != state.BumpFloor)
		{
			bump = state.BumpFloor;
			model->SetShaderMode(bump ? Engine::SurfaceShader::Bump
				: Engine::SurfaceShader::Directional);
			model->SetShaderDetailTexture(bump ? "assets/texture/Normal.png" : "");
		}
	}
};

}

void ShaderGalleryScene::Init()
{
	auto& world = GetWorld();
	world.Spawn<GalleryCamera>();
	world.Spawn<GalleryFloor>();
	for (int index = 0; index < static_cast<int>(Engine::ShaderGalleryModes.size()); ++index)
		world.Spawn<GalleryModel>(index);
}
void ShaderGalleryScene::Uninit() { Renderer::SetPostEffect(PostEffectMode::None); }
void ShaderGalleryScene::Update()
{
	auto& state = GetGalleryUiState();
	if (state.StartPresentation)
	{
		m_SavedState = state;
		state.Playing = true;
		state.PreviewSeconds = 0.0f;
		state.StartPresentation = false;
	}
	if (state.Playing)
	{
		state.PreviewSeconds += 1.0f / 60.0f;
		if (state.PreviewSeconds >= 22.0f || Keyboard_IsKeyDownTrigger(KK_SPACE))
		{
			state = m_SavedState;
			state.StartPresentation = false;
			state.Playing = false;
		}
		else
		{
			const float t = state.PreviewSeconds;
			state.Post = t < 8.0f ? 0 : t < 11.0f ? 8 : t < 13.0f ? 5
				: t < 15.0f ? 2 : t < 18.0f ? 9 : 8;
			state.Light.Position.x = -1.5f + 3.0f * (std::min)(t / 8.0f, 1.0f);
			state.Light.Direction = { 0.0f, -1.0f, 0.0f, 0.0f };
			state.Light.Diffuse = { 1.4f + 0.35f * std::sin(t * 1.8f),
				1.05f + 0.35f * std::sin(t * 1.8f + 1.0f), 1.25f, 1.0f };
			if (t >= 8.0f && t < 11.0f)
			{
				const float pulse = std::sin((t - 8.0f) * XM_PI / 3.0f);
				state.ShiftX = 0.003f + 0.038f * pulse;
				state.ShiftY = 0.010f * pulse;
			}
			else if (t >= 11.0f && t < 13.0f)
			{
				state.Noise = 0.1f + 0.13f * ((t - 11.0f) / 2.0f);
				state.Contrast = 0.8f;
			}
			else if (t >= 13.0f && t < 15.0f)
				state.BlockSize = 5.0f + 22.0f * std::sin((t - 13.0f) * XM_PIDIV2);
			else if (t >= 15.0f && t < 18.0f)
			{
				state.PosterLevels = 12.0f - 8.0f * ((t - 15.0f) / 3.0f);
				state.Dither = 0.35f;
				state.PosterContrast = 0.9f;
			}
			else if (t >= 18.0f)
			{
				state.ShiftX = 0.006f + 0.006f * (0.5f + 0.5f * std::sin(t * 4.0f));
				state.ShiftY = 0.003f;
			}
			state.ShowFloor = t < 8.0f;
		}
	}
	Renderer::SetPostEffect(static_cast<PostEffectMode>(state.Post));
	XMFLOAT4 options{ state.BlockSize, state.CircleRadius, 0.0f, 0.0f };
	XMFLOAT4 extra{};
	switch (state.Post)
	{
	case 5: options = { state.Noise, state.Contrast, state.PreviewSeconds, 0.0f }; break;
	case 6: options.x = state.MipLevel; break;
	case 7: options.x = state.ShapeScale; extra.x = static_cast<float>(state.Shape); break;
	case 8: options = { state.ShiftX, state.ShiftY, 0.0f, 0.0f }; break;
	case 9:
		options = { state.PosterLevels, state.Dither, state.PosterGray ? 1.0f : 0.0f, 0.0f };
		extra.x = state.PosterContrast;
		break;
	default: break;
	}
	Renderer::SetPostEffectParameters(options, extra);
}
void ShaderGalleryScene::Draw() {}
