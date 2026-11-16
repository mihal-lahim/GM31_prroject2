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
// SP31 のギャラリーと同じ背景色です(映像プレビュー中は暗くします)。
constexpr XMFLOAT4 GalleryClearColor{ 0.4f, 0.2f, 0.2f, 1.0f };
constexpr XMFLOAT4 PresentationClearColor{ 0.025f, 0.035f, 0.065f, 1.0f };

float SmoothStep(float value)
{
	value = std::clamp(value, 0.0f, 1.0f);
	return value * value * (3.0f - 2.0f * value);
}

// SP31 の Camera.cpp と同じ軌道カメラです。注視点の周りを回り、右ドラッグで注視点ごと動かします。
class GalleryCamera final : public GameObject, public ICameraView
{
	// SP31 の Game.cpp と同じく (0, 2.5, -7) から原点を見る向きで始めます。
	float yaw = 0.0f;
	float pitch = std::asin(2.5f / std::sqrt(55.25f));
	float radius = std::sqrt(55.25f);
	Vector3 target{};
	float savedYaw = 0.0f;
	float savedPitch = 0.0f;
	float savedRadius = 0.0f;
	Vector3 savedTarget{};
	bool wasPlaying = false;
	Vector3 position{};
	XMMATRIX view = XMMatrixIdentity();

	void UpdatePresentation(float seconds)
	{
		const float approach = SmoothStep(seconds / 2.0f);
		const float orbit = SmoothStep((seconds - 2.0f) / 6.0f);
		const float push = SmoothStep((seconds - 15.0f) / 3.0f);
		const float reveal = SmoothStep((seconds - 18.0f) / 4.0f);
		const float distance = seconds < 18.0f
			? 5.1f - 1.3f * approach - 0.7f * push
			: 4.9f - 0.7f * reveal;
		const float shake = seconds >= 8.0f && seconds < 15.0f ? 0.025f * std::sin(seconds * 27.0f) : 0.0f;
		const float angle = -0.38f + 0.84f * orbit + shake - 0.3f * reveal;
		target = { 0.0f, 0.75f, 0.0f };
		position = { distance * std::sin(angle), 1.9f + 0.18f * std::sin(seconds * 1.2f),
			-distance * std::cos(angle) };
	}

	void UpdateControls()
	{
		auto& state = GetGalleryUiState();
		float dx = 0.0f, dy = 0.0f;
		const bool dragged = Mouse_ConsumeDelta(&dx, &dy) && !ImGui::GetIO().WantCaptureMouse;
		const bool useKeyboard = !ImGui::GetIO().WantCaptureKeyboard;
		if (useKeyboard && Keyboard_IsKeyDown(KK_A)) yaw += XMConvertToRadians(0.3f);
		if (useKeyboard && Keyboard_IsKeyDown(KK_D)) yaw -= XMConvertToRadians(0.3f);
		if (dragged && Mouse_IsLeftButtonDown())
		{
			yaw += dx * 0.005f;
			pitch += dy * 0.005f;
		}
		const float pitchLimit = XMConvertToRadians(85.0f);
		pitch = std::clamp(pitch, -pitchLimit, pitchLimit);

		float zoom = state.WheelZoom;
		state.WheelZoom = 0.0f;
		if (useKeyboard && Keyboard_IsKeyDown(KK_W)) zoom += 0.12f;
		if (useKeyboard && Keyboard_IsKeyDown(KK_S)) zoom -= 0.12f;
		radius = std::clamp(radius * std::exp(-zoom * 0.12f), 0.75f, 80.0f);

		const float sy = std::sin(yaw), cy = std::cos(yaw);
		const float sp = std::sin(pitch), cp = std::cos(pitch);
		if (dragged && Mouse_IsRightButtonDown())
		{
			const float worldPerPixel = 2.0f * radius * std::tan(GalleryFieldOfView * 0.5f) / SCREEN_HEIGHT;
			const float right = -dx * worldPerPixel;
			const float up = dy * worldPerPixel;
			target = target + Vector3{ right * cy - up * sp * sy, up * cp, right * sy + up * sp * cy };
		}
		position = target + Vector3{ radius * cp * sy, radius * sp, -radius * cp * cy };
	}

public:
	void Init() override { m_Layer = Layer::Camera; }
	void PreUpdate() override
	{
		const auto& state = GetGalleryUiState();
		if (state.Playing)
		{
			if (!wasPlaying)
			{
				savedYaw = yaw; savedPitch = pitch; savedRadius = radius; savedTarget = target;
				wasPlaying = true;
			}
			UpdatePresentation(state.PreviewSeconds);
			return;
		}
		if (wasPlaying)
		{
			yaw = savedYaw; pitch = savedPitch; radius = savedRadius; target = savedTarget;
			wasPlaying = false;
		}
		UpdateControls();
	}
	void LateUpdate() override
	{
		view = XMMatrixLookAtLH(XMVectorSet(position.x, position.y, position.z, 1.0f),
			XMVectorSet(target.x, target.y, target.z, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		GetGalleryUiState().View = view;
	}
	void Draw() override
	{
		Engine::Renderer::SetLight(GetGalleryUiState().Light);
		Engine::Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(GalleryFieldOfView,
			static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, GalleryNearClip, GalleryFarClip));
		Engine::Renderer::SetViewMatrix(view);
		Engine::Renderer::SetCameraPosition({ position.x, position.y, position.z });
	}
	XMMATRIX GetViewMatrix() const override { return view; }
	Vector3 GetCameraPosition() const override { return position; }
	Vector3 GetCameraForward() const override { Vector3 forward = target - position; forward.normalize(); return forward; }
};

class GalleryModel final : public GameObject
{
	ModelRenderer* model = nullptr;
	int index = 0;

	void ApplyPlacement()
	{
		const auto& state = GetGalleryUiState();
		GalleryPlacement placement{};
		const bool visible = GetGalleryPlacement(state, index, placement);
		model->SetVisible(visible);
		if (!visible) return;
		const float scale = placement.Scale * GalleryModelUnit;
		SetPosition({ placement.Position.x, placement.Position.y, placement.Position.z });
		SetScale({ scale, scale, scale });
		// OBJ は穴がY軸向きで寝ているため、SP31 の model.fbx と同じく立ててからY軸で回します。
		SetRotation({ -XM_PIDIV2,
			XMConvertToRadians(state.RotationDegrees + placement.RotationOffsetDegrees), 0.0f });
	}

public:
	explicit GalleryModel(int value) : index(value) {}
	void Init() override
	{
		model = AddComponent<ModelRenderer>();
		model->Load("assets/model/shaderDonut.obj");
		model->SetUseOwnerTransform(true);
		const auto& mode = Engine::ShaderGalleryModes[index];
		if (mode.DefinitionId.empty() || !model->SetShaderDefinition(mode.DefinitionId))
			model->SetShaderMode(mode.Shader);
		if (index == 11 || index == 12) model->SetShaderDetailTexture("assets/texture/Toon2.png");
		if (index == 13) model->SetShaderDetailTexture("assets/texture/Normal.png");
		ApplyPlacement();
	}
	void Update() override
	{
		XMFLOAT4 parameter = GetGalleryUiState().Parameters[index];
		// 背面輪郭の幅はモデル空間の長さです。UI は SP31 の model.fbx 基準なので OBJ の単位へ直します。
		if (index == 12) parameter.w /= GalleryModelUnit;
		model->SetShaderParameter(parameter);
		ApplyPlacement();
	}
	// 床が専用ライトに切り替えることがあるため、モデルは毎回ギャラリーのライトで描く。
	void Draw() override { Engine::Renderer::SetLight(GetGalleryUiState().Light); }
};

// SP31 の Field3D(スポットライト)と BumpField3D(法線マップ)と同じ 4x4 の石畳です。
class GalleryFloor final : public GameObject
{
	ModelRenderer* model = nullptr;
	bool bump = false;

	void ApplyShader()
	{
		model->SetShaderDefinition(bump ? "sp31-bump-field" : "sp31-spot-field");
		if (bump) model->SetShaderDetailTexture("assets/texture/Normal.png");
	}

public:
	void Init() override
	{
		model = AddComponent<ModelRenderer>();
		model->Load("assets/model/galleryFloor.obj");
		model->SetUseOwnerTransform(true);
		ApplyShader();
	}
	void Update() override
	{
		const auto& state = GetGalleryUiState();
		model->SetVisible(state.ShowFloor);
		if (bump != state.BumpFloor)
		{
			bump = state.BumpFloor;
			ApplyShader();
		}
	}
	// 法線マップの床は SP31 の BumpField3D と同じく専用ライトで照らす。
	void Draw() override
	{
		const auto& state = GetGalleryUiState();
		Engine::Renderer::SetLight(bump ? state.BumpLight : state.Light);
	}
};

// SP31 の ShaderGallery::ApplyPresentation と同じライトと回転です。
void ApplyPresentationLight(GalleryUiState& state, float seconds)
{
	const float sweep = SmoothStep(seconds / 8.0f);
	const float lightX = seconds < 8.0f ? -1.5f + 3.0f * sweep : 1.2f * std::sin((seconds - 8.0f) * 1.7f);
	state.Light.Position = { lightX, 3.2f, -0.35f, 1.0f };
	state.Light.Direction = { 0.0f, -1.0f, 0.0f, 0.0f };
	state.Light.Angle.x = XMConvertToRadians(55.0f);
	state.Light.PointLightparam.x = 8.0f;
	state.Light.PointLightparam.y = 1.5f;
	state.Light.Ambient = { 0.21f, 0.18f, 0.29f, 1.0f };
	const float colorPhase = 0.5f + 0.5f * std::sin(seconds * 1.8f);
	state.Light.Diffuse = { 1.65f - 0.65f * colorPhase, 0.75f + 0.95f * colorPhase,
		1.35f + 0.45f * (1.0f - colorPhase), 1.0f };
	const float early = (std::min)(seconds, 8.0f);
	const float middle = seconds < 18.0f ? (std::max)(seconds - 8.0f, 0.0f) : 10.0f;
	const float finale = (std::max)(seconds - 18.0f, 0.0f);
	state.RotationDegrees = early * 42.0f + middle * 110.0f + finale * 65.0f;
}

}

void ShaderGalleryScene::Init()
{
	Engine::Renderer::SetClearColor(GalleryClearColor);
	auto& world = GetWorld();
	world.Spawn<GalleryCamera>();
	world.Spawn<GalleryFloor>();
	for (int index = 0; index < static_cast<int>(Engine::ShaderGalleryModes.size()); ++index)
		world.Spawn<GalleryModel>(index);
}
void ShaderGalleryScene::Uninit()
{
	Engine::Renderer::SetPostEffect(Engine::PostEffectMode::None);
	Engine::Renderer::SetClearColor(Engine::Renderer::DefaultClearColor);
}
void ShaderGalleryScene::Update()
{
	auto& state = GetGalleryUiState();
	m_ElapsedSeconds += 1.0f / 60.0f;
	if (state.StartPresentation)
	{
		m_SavedState = state;
		state.Playing = true;
		state.PreviewSeconds = 0.0f;
		state.StartPresentation = false;
		state.BumpFloor = false;
		state.ShowRamp = false;
		state.ShowNames = false;
		state.Animate = true;
		state.RotationDegrees = 0.0f;
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
			state.Selected = GetGalleryPresentationMode(t);
			state.Post = t < 8.0f ? 0 : t < 11.0f ? 8 : t < 13.0f ? 5
				: t < 15.0f ? 2 : t < 18.0f ? 9 : 8;
			ApplyPresentationLight(state, t);
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
				state.PosterGray = false;
			}
			else if (t >= 18.0f)
			{
				state.ShiftX = 0.006f + 0.006f * (0.5f + 0.5f * std::sin(t * 4.0f));
				state.ShiftY = 0.003f;
			}
			state.ShowFloor = t < 8.0f;
		}
	}
	else if (state.Animate) state.RotationDegrees += 20.0f / 60.0f;
	Engine::Renderer::SetClearColor(state.Playing ? PresentationClearColor : GalleryClearColor);
	if (state.Post == GalleryBloomPost)
	{
		if (Engine::Renderer::SetPostEffectDefinition("post-bloom"))
		{
			Engine::Renderer::SetPostEffectFloat("luminance", state.BloomLuminance);
			Engine::Renderer::SetPostEffectFloat("blendFine", state.BloomBlend[0]);
			Engine::Renderer::SetPostEffectFloat("blendMiddle", state.BloomBlend[1]);
			Engine::Renderer::SetPostEffectFloat("blendWide", state.BloomBlend[2]);
		}
		else Engine::Renderer::SetPostEffect(Engine::PostEffectMode::None);
		return;
	}
	Engine::Renderer::SetPostEffect(static_cast<Engine::PostEffectMode>(state.Post));
	XMFLOAT4 options{ state.BlockSize, state.CircleRadius, 0.0f, 0.0f };
	XMFLOAT4 extra{};
	switch (state.Post)
	{
	case 5: options = { state.Noise, state.Contrast, m_ElapsedSeconds, 0.0f }; break;
	case 6: options.x = state.MipLevel; break;
	case 7: options.x = state.ShapeScale; extra.x = static_cast<float>(state.Shape); break;
	case 8: options = { state.ShiftX, state.ShiftY, 0.0f, 0.0f }; break;
	case 9:
		options = { state.PosterLevels, state.Dither, state.PosterGray ? 1.0f : 0.0f, 0.0f };
		extra.x = state.PosterContrast;
		break;
	default: break;
	}
	Engine::Renderer::SetPostEffectParameters(options, extra);
}
void ShaderGalleryScene::Draw() {}
