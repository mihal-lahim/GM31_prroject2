#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "inputActions.h"
#include "DirectXTex.h"
#include "classroomWalk.h"
#include "animationModel.h"
#include "cloth.h"
#include "collider.h"
#include "gameTime.h"
#include "sky.h"
#include "keyboard_utf8.h"
#include "manager.h"
#include "modelRenderer.h"
#include "renderQueue.h"
#include "renderer.h"
#include "world.h"
#include "assimp/cimport.h"
#include "assimp/config.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

namespace
{
	// のれんの変形を横から確認するための固定カメラです。照明は ClassroomCamera と同じにします。
	class NorenSideCamera final : public GameObject, public ICameraView
	{
	public:
		void Init() override { m_Layer = Layer::Camera; }
		void Place(const Vector3& eye, const Vector3& target)
		{
			SetPosition(eye);
			m_Target = target;
			m_View = XMMatrixLookAtLH(XMVectorSet(eye.x, eye.y, eye.z, 1.0f),
				XMVectorSet(target.x, target.y, target.z, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		}
		void Draw() override
		{
			Engine::LIGHT light{};
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
			Engine::Renderer::SetLight(light);
			Engine::Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(0.8f,
				static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 200.0f));
			Engine::Renderer::SetViewMatrix(m_View);
			Engine::Renderer::SetCameraPosition(XMFLOAT3(GetPosition().x, GetPosition().y, GetPosition().z));
		}
		XMMATRIX GetViewMatrix() const override { return m_View; }
		Vector3 GetCameraPosition() const override { return GetPosition(); }
		Vector3 GetCameraForward() const override
		{
			Vector3 forward = m_Target - GetPosition();
			forward.normalize();
			return forward;
		}

	private:
		XMMATRIX m_View = XMMatrixIdentity();
		Vector3 m_Target{};
	};
}

bool RunClassroomRenderTest()
{
	std::ofstream log("render-test-classroom.log");
	aiPropertyStore* properties = aiCreatePropertyStore();
	bool legChainsValid = false;
	if (properties != nullptr)
	{
		aiSetImportPropertyInteger(properties, AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, 0);
		const aiScene* skeleton = aiImportFileExWithProperties(
			"assets/model/Akai.fbx", aiProcess_ConvertToLeftHanded, nullptr, properties);
		if (skeleton != nullptr && skeleton->mRootNode != nullptr)
		{
			legChainsValid = true;
			for (const char* side : { "Left", "Right" })
			{
				const std::string prefix = std::string("mixamorig:") + side;
				const aiNode* upper = skeleton->mRootNode->FindNode((prefix + "UpLeg").c_str());
				const aiNode* lower = skeleton->mRootNode->FindNode((prefix + "Leg").c_str());
				const aiNode* foot = skeleton->mRootNode->FindNode((prefix + "Foot").c_str());
				legChainsValid &= upper != nullptr && lower != nullptr && foot != nullptr
					&& lower->mParent == upper && foot->mParent == lower;
			}
			aiReleaseImport(skeleton);
		}
		aiReleasePropertyStore(properties);
	}
	auto frames = [](int count)
	{
		for (int i = 0; i < count; ++i)
		{
			Engine::Manager::Update(1.0f / 60.0f);
			keycopy();
			Engine::Manager::Draw();
		}
	};
	auto capture = [](const wchar_t* path)
	{
		ComPtr<ID3D11RenderTargetView> target;
		Engine::Renderer::GetDeviceContext()->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		if (!target) return false;
		ComPtr<ID3D11Resource> resource;
		target->GetResource(resource.GetAddressOf());
		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(Engine::Renderer::GetDevice(),
			Engine::Renderer::GetDeviceContext(), resource.Get(), image))) return false;
		return SUCCEEDED(DirectX::SaveToWICFile(*image.GetImage(0, 0, 0),
			DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path));
	};

	// Every directed clip pair, including interruption before a blend finishes.
	int transitionsPassed = 0, interruptionsPassed = 0, loopsPassed = 0;
	bool jumpHolds = false;
	{
		World animationWorld;
		auto* object = animationWorld.Spawn<GameObject>();
		auto* model = object->AddComponent<AnimationModel>();
		model->Load("assets/model/Akai.fbx");
		model->SetModelScale(0.01f);
		model->SetModelYawOffset(XM_PI);
		animationWorld.Spawn<MeshField>();
		auto* animationCamera = animationWorld.Spawn<ClassroomCamera>();
		animationCamera->LateUpdate();
		const char* names[] = { "Idle", "Run", "Jump", "TreadWater", "Swim", "Flair" };
		const char* files[] = { "Akai_Idle.fbx", "Akai_Run.fbx", "Jumping.fbx",
			"Treading Water.fbx", "Swimming To Edge.fbx", "Flair.fbx" };
		double durations[6]{};
		for (int i = 0; i < 6; ++i)
		{
			const std::string path = std::string("assets/model/") + files[i];
			model->LoadAnimation(path.c_str(), names[i]);
			const aiScene* scene = aiImportFile(path.c_str(), aiProcess_ConvertToLeftHanded);
			if (scene && scene->mNumAnimations)
			{
				const aiAnimation* a = scene->mAnimations[0];
				durations[i] = a->mDuration / (a->mTicksPerSecond > 0 ? a->mTicksPerSecond : 25);
			}
			if (scene) aiReleaseImport(scene);
		}
		auto configure = [&](int i)
		{
			model->SetPlayback(1.0f, i == 2 ? 0.36f : 0.0f,
				i == 2 ? 0.62f : i == 4 ? 0.18f : 1.0f, i != 2);
		};
		auto difference = [](const auto& a, const auto& b)
		{
			float result = 0.0f;
			if (a.empty() || a.size() != b.size()) return 1e9f;
			for (const auto& [name, matrix] : a)
			{
				const auto found = b.find(name);
				if (found == b.end()) return 1e9f;
				for (int row = 0; row < 4; ++row)
					for (int column = 0; column < 4; ++column)
					{
						const float d = std::abs(matrix[row][column] - found->second[row][column]);
						if (!std::isfinite(d)) return 1e9f;
						result = (std::max)(result, d);
					}
			}
			return result;
		};
		for (int from = 0; from < 6; ++from)
			for (int to = 0; to < 6; ++to)
			{
				if (from == to) continue;
				configure(from);
				model->PlayAnimation(names[from], 0.0f);
				model->TickAnimation(0.3f, false);
				const auto source = model->GetAnimationPose();
				configure(to);
				model->PlayAnimation(names[to], 0.2f);
				model->TickAnimation(0.0f, false);
				const bool continuous = difference(source, model->GetAnimationPose()) < 0.001f;
				model->TickAnimation(0.1f, false);
				const auto middle = model->GetAnimationPose();
				model->TickAnimation(0.0f);
				Engine::Renderer::Begin();
				RenderQueue queue;
				queue.Build(animationWorld, animationCamera);
				queue.Execute();
				Engine::Renderer::End();
				const std::string imageName = std::string("transition-") + names[from] + "-" + names[to] + ".png";
				capture(std::wstring(imageName.begin(), imageName.end()).c_str());
				const float progress = difference(source, middle);
				transitionsPassed += continuous && progress > 0.00001f && progress < 1e8f;
				const int next = (to + 1) % 6;
				configure(next);
				model->PlayAnimation(names[next], 0.2f);
				model->TickAnimation(0.0f, false);
				interruptionsPassed += difference(middle, model->GetAnimationPose()) < 0.001f;
			}
		for (int i = 0; i < 6; ++i)
		{
			configure((i + 1) % 6);
			model->PlayAnimation(names[(i + 1) % 6], 0.0f);
			model->TickAnimation(0.0f, false);
			configure(i);
			model->PlayAnimation(names[i], 0.0f);
			const float length = static_cast<float>(durations[i] * (i == 2 ? 0.26 : i == 4 ? 0.18 : 1.0));
			model->TickAnimation((std::max)(0.0f, length - 0.008f), false);
			const auto end = model->GetAnimationPose();
			model->TickAnimation(0.016f, false);
			if (i != 2) loopsPassed += length > 0 && difference(end, model->GetAnimationPose()) < 0.001f;
			else
			{
				const auto held = model->GetAnimationPose();
				model->TickAnimation(1.0f, false);
				jumpHolds = difference(held, model->GetAnimationPose()) < 0.001f;
			}
		}
	}
	log << "animation transitions: " << transitionsPassed << "/30\n"
		<< "animation interruptions: " << interruptionsPassed << "/30\n"
		<< "animation loop seams: " << loopsPassed << "/5\n"
		<< "jump holds without looping: " << jumpHolds << '\n';
	frames(60);
	const bool idleCaptured = capture(L"classroom-idle.png");
	Keyboard_ProcessMessage(WM_KEYDOWN, VK_SPACE, 0);
	frames(8);
	Keyboard_ProcessMessage(WM_KEYUP, VK_SPACE, 0);
	const bool jumpRiseCaptured = capture(L"classroom-jump-rise.png");
	frames(12);
	const bool jumpPeakCaptured = capture(L"classroom-jump-peak.png");
	frames(25);
	const bool jumpLandedCaptured = capture(L"classroom-jump-landed.png");
	frames(30);
	const bool windCaptured = capture(L"classroom-grass-wind.png");
	Keyboard_ProcessMessage(WM_KEYDOWN, 'W', 0);
	frames(1);
	const bool startCaptured = capture(L"classroom-run-start.png");
	frames(6);
	const bool blendToRunCaptured = capture(L"classroom-blend-to-run.png");
	frames(38);
	Keyboard_ProcessMessage(WM_KEYUP, 'W', 0);
	const bool walkingCaptured = capture(L"classroom-walking.png");
	frames(6);
	const bool blendToIdleCaptured = capture(L"classroom-blend-to-idle.png");
	frames(20);
	const bool stoppedCaptured = capture(L"classroom-stopped.png");
	Keyboard_ProcessMessage(WM_KEYDOWN, 'W', 0);
	Keyboard_ProcessMessage(WM_KEYDOWN, 'A', 0);
	frames(140);
	Keyboard_ProcessMessage(WM_KEYUP, 'W', 0);
	Keyboard_ProcessMessage(WM_KEYUP, 'A', 0);
	const bool slopeCaptured = capture(L"classroom-slope.png");
	frames(25);
	const bool slopeStoppedCaptured = capture(L"classroom-slope-stopped.png");
	const bool terrainHasSlope = MeshField::GetHeight(-12.0f, 13.0f)
		> MeshField::GetHeight(0.0f, 0.0f) + 0.5f;
	bool steepHill = false;
	for (int z = 18; z <= 20; ++z)
		steepHill |= MeshField::GetNormal(0.0f, static_cast<float>(z) + 0.25f).y < 0.82f;
	const bool lakeFloorBelowWater = MeshField::GetHeight(ClassroomLake::CenterX, ClassroomLake::CenterZ)
		< ClassroomLake::WaterLevel - 1.0f;
	const bool lakeBankAboveWater = MeshField::GetHeight(ClassroomLake::CenterX + ClassroomLake::WaterRadius,
		ClassroomLake::CenterZ) > ClassroomLake::WaterLevel;
	World contactWorld;
	ClassroomPlayer* contactPlayer = contactWorld.Spawn<ClassroomPlayer>();
	auto placeOnGround = [&](float x, float z)
	{
		contactPlayer->SetPosition({ x, MeshField::GetHeight(x, z), z });
		for (int i = 0; i < 30; ++i) { Engine::InputActions::Update(); contactPlayer->Update(); }
	};
	float flatClearance[2]{}, hillClearance[2]{}, sideClearance[2]{};
	Engine::InputActions::Update();
	contactPlayer->Update();
	const bool flatMeasured = contactPlayer->GetFootClearance(flatClearance);
	// Ground-contact samples must be on dry land now that the lake has buoyancy.
	placeOnGround(-12.0f, 13.0f);
	const bool hillMeasured = contactPlayer->GetFootClearance(hillClearance);
	contactPlayer->SetRotation({ 0.0f, -XM_PIDIV4, 0.0f });
	placeOnGround(-14.0f, 11.0f);
	const bool sideMeasured = contactPlayer->GetFootClearance(sideClearance);
	contactPlayer->SetRotation({ 0.0f, 0.0f, 0.0f });
	placeOnGround(0.0f, 19.0f);
	float slopeEndClearance[2][2]{};
	const bool slopeEndsMeasured = contactPlayer->GetFootEndClearance(slopeEndClearance);
	bool slopeEndsPlanted = slopeEndsMeasured
		&& MeshField::GetNormal(0.0f, 19.0f).y < 0.85f;
	for (const auto& foot : slopeEndClearance)
		for (float gap : foot)
			slopeEndsPlanted &= std::abs(gap) < 0.035f;
	contactWorld.Spawn<MeshField>();
	MeshField* comparisonField = contactWorld.FindFirst<MeshField>();
	ClassroomCamera* slopeCamera = contactWorld.Spawn<ClassroomCamera>();
	slopeCamera->LateUpdate();
	Engine::Renderer::Begin();
	RenderQueue slopeQueue;
	slopeQueue.Build(contactWorld, slopeCamera);
	slopeQueue.Execute();
	Engine::Renderer::End();
	const bool idleSlopeCaptured = capture(L"classroom-idle-slope.png");
	// 同じ地形とカメラを描き直し、描画対象のシェーダー切替が画素に効くことを検証する。
	auto terrainPixels = [&]()
	{
		Engine::Renderer::Begin();
		slopeQueue.Build(contactWorld, slopeCamera);
		slopeQueue.Execute();
		Engine::Renderer::End();
		ComPtr<ID3D11RenderTargetView> target;
		Engine::Renderer::GetDeviceContext()->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		if (!target) return std::vector<BYTE>{};
		ComPtr<ID3D11Resource> resource;
		target->GetResource(resource.GetAddressOf());
		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(Engine::Renderer::GetDevice(),
			Engine::Renderer::GetDeviceContext(), resource.Get(), image))) return std::vector<BYTE>{};
		const DirectX::Image* frame = image.GetImage(0, 0, 0);
		return std::vector<BYTE>(frame->pixels, frame->pixels + frame->slicePitch);
	};
	const std::vector<BYTE> defaultPixels = terrainPixels();
	auto difference = [](const std::vector<BYTE>& left, const std::vector<BYTE>& right)
	{
		std::size_t count = 0;
		if (left.size() == right.size())
			for (std::size_t i = 0; i < left.size(); ++i) count += left[i] != right[i];
		return count;
	};
	const bool animatedDefinitionLoaded = contactPlayer->SetShaderDefinition("sample-stripes");
	const std::vector<BYTE> animatedRegisteredPixels = terrainPixels();
	const bool animatedRegisteredCaptured = capture(L"classroom-animated-stripes.png");
	const std::size_t animatedChangedBytes = difference(defaultPixels, animatedRegisteredPixels);
	const bool animatedRegisteredVisible = animatedDefinitionLoaded && animatedRegisteredCaptured
		&& animatedChangedBytes > 1000;
	contactPlayer->UseDefaultShader();
	const bool postDefinitionLoaded = Engine::Renderer::SetPostEffectDefinition("post-invert");
	const std::vector<BYTE> postDefinitionPixels = terrainPixels();
	const bool postDefinitionCaptured = capture(L"classroom-post-invert.png");
	const bool postParameterChanged = Engine::Renderer::SetPostEffectFloat("strength", 0.0f);
	const std::vector<BYTE> postParameterPixels = terrainPixels();
	const std::size_t postDefinitionChangedBytes = difference(defaultPixels, postDefinitionPixels);
	const bool postDefinitionVisible = postDefinitionLoaded && postDefinitionCaptured
		&& postParameterChanged && postDefinitionChangedBytes > 50000
		&& difference(postDefinitionPixels, postParameterPixels) > 50000;
	Engine::Renderer::SetPostEffect(Engine::PostEffectMode::None);
	int registeredPostCount = 0;
	int visibleRegisteredPostCount = 0;
	for (const Engine::ShaderDefinition& definition : Engine::ShaderCatalog)
	{
		if (definition.Usage != Engine::ShaderUsage::Post) continue;
		++registeredPostCount;
		if (Engine::Renderer::SetPostEffectDefinition(definition.Id)
			&& difference(defaultPixels, terrainPixels()) > 50000)
			++visibleRegisteredPostCount;
		Engine::Renderer::SetPostEffect(Engine::PostEffectMode::None);
	}
	const bool allRegisteredPostsVisible = registeredPostCount > 0
		&& visibleRegisteredPostCount == registeredPostCount;
	const bool registeredLoaded = comparisonField->SetShaderDefinition("sample-stripes");
	const std::vector<BYTE> registeredPixels = terrainPixels();
	const bool registeredCaptured = capture(L"classroom-sample-stripes.png");
	const std::size_t registeredChangedBytes = difference(defaultPixels, registeredPixels);
	const bool registeredVisible = registeredLoaded && registeredCaptured
		&& registeredChangedBytes > 50000;
	comparisonField->UseRealisticShader();
	Engine::Renderer::SetPostEffect(Engine::PostEffectMode::Grayscale);
	const std::vector<BYTE> postPixels = terrainPixels();
	const bool postCaptured = capture(L"classroom-post-grayscale.png");
	Engine::Renderer::SetPostEffect(Engine::PostEffectMode::Mosaic);
	const std::vector<BYTE> mosaicPixels = terrainPixels();
	const bool mosaicCaptured = capture(L"classroom-post-mosaic.png");
	Engine::Renderer::SetPostEffect(Engine::PostEffectMode::MipPreview);
	const std::vector<BYTE> mipPixels = terrainPixels();
	const bool mipCaptured = capture(L"classroom-post-mip.png");
	Engine::Renderer::SetPostEffect(Engine::PostEffectMode::None);
	const std::size_t postChangedBytes = difference(defaultPixels, postPixels);
	const bool postVisible = postCaptured && postChangedBytes > 50000;
	const std::size_t mosaicChangedBytes = difference(defaultPixels, mosaicPixels);
	const bool mosaicVisible = mosaicCaptured && mosaicChangedBytes > 50000;
	const bool mipVisible = mipCaptured && difference(defaultPixels, mipPixels) > 50000;
	const bool normalMapLoaded = Engine::ResourceManager::LoadTexture(L"assets/texture/Normal.png") != nullptr;
	const bool bumpSelected = comparisonField->SetShaderMode(Engine::SurfaceShader::Bump);
	const std::vector<BYTE> bumpPixels = terrainPixels();
	const bool bumpCaptured = capture(L"classroom-bump.png");
	const bool bumpVisible = normalMapLoaded && bumpSelected && bumpCaptured
		&& difference(defaultPixels, bumpPixels) > 50000;
	const bool rampLoaded = Engine::ResourceManager::LoadTexture(L"assets/texture/Toon2.png") != nullptr;
	const bool rampSelected = comparisonField->SetShaderMode(Engine::SurfaceShader::ToonRamp);
	const std::vector<BYTE> rampPixels = terrainPixels();
	const bool rampCaptured = capture(L"classroom-toon-ramp.png");
	const bool rampVisible = rampLoaded && rampSelected && rampCaptured
		&& difference(defaultPixels, rampPixels) > 50000;
	const bool shaderChanged = comparisonField->SetShaderMode(Engine::SurfaceShader::Unlit);
	const std::vector<BYTE> unlitPixels = terrainPixels();
	const std::size_t changedBytes = difference(defaultPixels, unlitPixels);
	const bool shaderVisible = shaderChanged && changedBytes > 50000;
	constexpr std::array<Engine::SurfaceShader, 10> lightingModes{ {
		Engine::SurfaceShader::VertexDirectional, Engine::SurfaceShader::Directional,
		Engine::SurfaceShader::BlinnPhong, Engine::SurfaceShader::Hemisphere,
		Engine::SurfaceShader::Point, Engine::SurfaceShader::Rim,
		Engine::SurfaceShader::Spot, Engine::SurfaceShader::Toon,
		Engine::SurfaceShader::CookTorrance, Engine::SurfaceShader::Disney
	} };
	int workingLightingModes = 0;
	for (const Engine::SurfaceShader mode : lightingModes)
	{
		if (!comparisonField->SetShaderMode(mode)) continue;
		const std::vector<BYTE> pixels = terrainPixels();
		if (difference(unlitPixels, pixels) > 50000) ++workingLightingModes;
	}
	const bool lightingModesVisible = workingLightingModes == static_cast<int>(lightingModes.size());
	comparisonField->UseRealisticShader();
	GameObject* outlineObject = contactWorld.Spawn<GameObject>();
	outlineObject->SetPosition({ 3.0f, MeshField::GetHeight(3.0f, 23.0f) + 0.42f, 23.0f });
	outlineObject->SetScale({ 0.015f, 0.015f, 0.015f });
	ModelRenderer* outlineModel = outlineObject->AddComponent<ModelRenderer>();
	outlineModel->Load("assets/model/shaderDonut.obj");
	outlineModel->SetUseOwnerTransform(true);
	outlineModel->SetShaderMode(Engine::SurfaceShader::ToonRamp,
		{ 4.0f, 0.02f, 0.65f, 0.0f });
	outlineModel->SetShaderDetailTexture("assets/texture/Toon2.png");
	const std::vector<BYTE> noOutlinePixels = terrainPixels();
	int registeredSurfaceCount = 0;
	int visibleRegisteredSurfaceCount = 0;
	for (const Engine::ShaderDefinition& definition : Engine::ShaderCatalog)
	{
		if (definition.Usage != Engine::ShaderUsage::Surface) continue;
		++registeredSurfaceCount;
		if (outlineModel->SetShaderDefinition(definition.Id)
			&& difference(noOutlinePixels, terrainPixels()) > 500)
			++visibleRegisteredSurfaceCount;
	}
	const bool allRegisteredSurfacesVisible = registeredSurfaceCount > 0
		&& visibleRegisteredSurfaceCount == registeredSurfaceCount;
	const bool namedTextureDefinitionLoaded = outlineModel->SetShaderDefinition("toon-outline");
	const std::vector<BYTE> noDetailPixels = terrainPixels();
	const bool namedTextureLoaded = outlineModel->SetShaderTexture(
		"detail", "assets/texture/Toon2.png");
	const std::vector<BYTE> detailPixels = terrainPixels();
	const std::size_t namedTextureChangedBytes = difference(noDetailPixels, detailPixels);
	const bool namedTextureVisible = namedTextureDefinitionLoaded && namedTextureLoaded
		&& namedTextureChangedBytes > 500;
	const bool modelDefinitionLoaded = outlineModel->SetShaderDefinition("sample-stripes");
	const std::vector<BYTE> modelRegisteredPixels = terrainPixels();
	const bool modelParameterChanged = outlineModel->SetShaderFloat("stripeCount", 5.0f);
	const std::vector<BYTE> modelParameterPixels = terrainPixels();
	const std::size_t modelRegisteredChangedBytes = difference(noOutlinePixels, modelRegisteredPixels);
	const bool modelRegisteredVisible = modelDefinitionLoaded && modelParameterChanged
		&& modelRegisteredChangedBytes > 500
		&& difference(modelRegisteredPixels, modelParameterPixels) > 500;
	const bool namedAlbedoLoaded = outlineModel->SetShaderTexture(
		"albedo", "assets/model/gravel.jpg");
	const std::vector<BYTE> namedAlbedoPixels = terrainPixels();
	const bool namedAlbedoVisible = namedAlbedoLoaded
		&& difference(modelParameterPixels, namedAlbedoPixels) > 500;
	outlineModel->SetShaderMode(Engine::SurfaceShader::ToonOutline,
		{ 4.0f, 0.02f, 0.65f, 0.0f });
	outlineModel->SetShaderDetailTexture("assets/texture/Toon2.png");
	const std::vector<BYTE> outlinePixels = terrainPixels();
	const bool outlineCaptured = capture(L"classroom-toon-outline.png");
	const std::size_t outlineChangedBytes = difference(noOutlinePixels, outlinePixels);
	const bool outlineVisible = outlineCaptured && outlineChangedBytes > 500;
	struct SlopeSample { float x, z, yaw; };
	constexpr std::array<SlopeSample, 4> slopeSamples{ {
		{ 0.0f, 19.0f, 0.0f }, { 0.0f, 3.0f, XM_PI },
		{ 8.0f, 11.0f, XM_PIDIV2 }, { -8.0f, 11.0f, -XM_PIDIV2 }
	} };
	float worstSlopeEndGap = 0.0f;
	bool bankSolesPlanted = true;
	for (const SlopeSample& sample : slopeSamples)
	{
		contactPlayer->SetRotation({ 0.0f, sample.yaw, 0.0f });
		placeOnGround(sample.x, sample.z);
		float ends[2][2]{};
		bankSolesPlanted &= contactPlayer->GetFootEndClearance(ends);
		for (const auto& foot : ends)
			for (float gap : foot)
				worstSlopeEndGap = (std::max)(worstSlopeEndGap, std::abs(gap));
	}
	bankSolesPlanted &= worstSlopeEndGap < 0.035f;
	// Exercise input and environment selection as well as the blend component.
	int playerStatesPassed = 0;
	auto updatePlayer = [&](int count)
	{
		for (int i = 0; i < count; ++i) { Engine::InputActions::Update(); contactPlayer->Update(); keycopy(); }
	};
	auto expectState = [&](std::string_view name)
	{
		playerStatesPassed += contactPlayer->GetAnimationName() == name;
	};
	placeOnGround(0.0f, 0.0f);
	expectState("Idle");
	Keyboard_ProcessMessage(WM_KEYDOWN, 'W', 0); updatePlayer(1); expectState("Run");
	Keyboard_ProcessMessage(WM_KEYDOWN, 'R', 0); updatePlayer(1); expectState("Flair");
	Keyboard_ProcessMessage(WM_KEYUP, 'R', 0); updatePlayer(1);
	Keyboard_ProcessMessage(WM_KEYDOWN, 'R', 0); updatePlayer(1); expectState("Run");
	Keyboard_ProcessMessage(WM_KEYUP, 'R', 0);
	Keyboard_ProcessMessage(WM_KEYUP, 'W', 0); updatePlayer(1); expectState("Idle");
	Keyboard_ProcessMessage(WM_KEYDOWN, 'R', 0); updatePlayer(1); expectState("Flair");
	Keyboard_ProcessMessage(WM_KEYUP, 'R', 0); updatePlayer(1);
	Keyboard_ProcessMessage(WM_KEYDOWN, 'R', 0); updatePlayer(1); expectState("Idle");
	Keyboard_ProcessMessage(WM_KEYUP, 'R', 0);
	Keyboard_ProcessMessage(WM_KEYDOWN, VK_SPACE, 0); updatePlayer(1); expectState("Jump");
	Keyboard_ProcessMessage(WM_KEYUP, VK_SPACE, 0); updatePlayer(55); expectState("Idle");
	contactPlayer->SetPosition({ ClassroomLake::CenterX, ClassroomLake::WaterLevel - 1.0f, ClassroomLake::CenterZ });
	updatePlayer(1); expectState("TreadWater");
	Keyboard_ProcessMessage(WM_KEYDOWN, 'W', 0); updatePlayer(1); expectState("Swim");
	Keyboard_ProcessMessage(WM_KEYUP, 'W', 0); updatePlayer(1); expectState("TreadWater");
	placeOnGround(0.0f, 0.0f); expectState("Idle");
	log << "player input/environment states: " << playerStatesPassed << "/13\n";
	contactPlayer->SetPosition({ 0.0f, ClassroomLake::WaterLevel - 1.1f, ClassroomLake::CenterZ - 3.0f });
	contactWorld.Spawn<ClassroomWater>();
	comparisonField->UseRealisticShader();
	Engine::Renderer::SetPostEffect(Engine::PostEffectMode::None);
	auto captureWater = [&](const wchar_t* path)
	{
		slopeCamera->LateUpdate();
		Engine::Renderer::Begin();
		slopeQueue.Build(contactWorld, slopeCamera);
		slopeQueue.Execute();
		Engine::Renderer::End();
		return capture(path);
	};
	Keyboard_ProcessMessage(WM_KEYDOWN, 'W', 0);
	float minimumChest = 1000.0f, maximumChest = -1000.0f;
	int chestSamples = 0;
	for (int i = 0; i < 150; ++i)
	{
		Engine::GameTime::Update(1.0f / 60.0f);
		updatePlayer(1);
		Vector3 chest;
		if (i >= 30 && contactPlayer->GetSwimAnchor(chest))
		{
			const float depth = chest.y - ClassroomWater::GetSurfaceHeight(chest.x, chest.z, Engine::GameTime::GetElapsedTime());
			minimumChest = (std::min)(minimumChest, depth);
			maximumChest = (std::max)(maximumChest, depth);
			++chestSamples;
		}
	}
	const bool swimmingDepthCaptured = captureWater(L"classroom-swim-depth.png");
	Keyboard_ProcessMessage(WM_KEYUP, 'W', 0); keycopy();
	log << "swim chest relative to water: " << minimumChest << " .. " << maximumChest << '\n';
	const bool swimDepthValid = chestSamples == 120 && maximumChest < 0.0f && minimumChest > -0.35f;
	minimumChest = 1000.0f; maximumChest = -1000.0f; chestSamples = 0;
	for (int i = 0; i < 150; ++i)
	{
		Engine::GameTime::Update(1.0f / 60.0f);
		updatePlayer(1);
		Vector3 chest;
		if (i >= 30 && contactPlayer->GetSwimAnchor(chest))
		{
			const float depth = chest.y - ClassroomWater::GetSurfaceHeight(chest.x, chest.z, Engine::GameTime::GetElapsedTime());
			minimumChest = (std::min)(minimumChest, depth);
			maximumChest = (std::max)(maximumChest, depth);
			++chestSamples;
		}
	}
	const bool treadingDepthCaptured = captureWater(L"classroom-tread-depth.png");
	const bool treadDepthValid = chestSamples == 120 && maximumChest < 0.0f && minimumChest > -0.35f;
	log << "tread chest relative to water: " << minimumChest << " .. " << maximumChest << '\n';
	log << "water body depth valid: " << swimDepthValid << '/' << treadDepthValid << '\n';
	placeOnGround(0.0f, 0.0f);
	float exitClearance[2]{};
	const bool shoreExitValid = contactPlayer->GetAnimationName() == "Idle"
		&& contactPlayer->GetFootClearance(exitClearance)
		&& std::abs(exitClearance[0]) < 0.035f && std::abs(exitClearance[1]) < 0.035f;
	log << "water alignment cleared on shore: " << shoreExitValid << '\n';
	// のれん: プレイヤーが歩いてくぐると布が押しのけられ、体にめり込まず、通り過ぎると元へ戻る。
	bool norenRestCaptured = false, norenContactCaptured = false, norenAfterCaptured = false;
	bool norenEnterCaptured = false, norenSideCaptured = false;
	bool norenPenetrationFree = true, norenFinite = false, norenWalkerPassed = false;
	float norenPushed = 0.0f, norenRemaining = 1e9f, norenWorstGap = 1e9f;
	{
		World norenWorld;
		norenWorld.Spawn<Sky>()->SetRadius(90.0f);
		norenWorld.Spawn<MeshField>();
		norenWorld.Spawn<ClassroomWater>();
		ClassroomNoren* noren = norenWorld.Spawn<ClassroomNoren>();
		ClassroomPlayer* walker = norenWorld.Spawn<ClassroomPlayer>();
		// ClassroomCameraは置きません(ビュー行列を上書きし合うため)。プレイヤーの移動はWで+Zです。
		NorenSideCamera* norenCamera = norenWorld.Spawn<NorenSideCamera>();
		Engine::Cloth* cloth = noren->GetCloth();
		const float norenGround = MeshField::GetHeight(ClassroomNoren::PositionX, ClassroomNoren::PositionZ);
		auto viewFromBehind = [&]()
		{
			norenCamera->Place({ 1.4f, norenGround + 2.2f, ClassroomNoren::PositionZ - 3.6f },
				{ 0.0f, norenGround + 1.4f, ClassroomNoren::PositionZ });
		};
		auto viewFromSide = [&]()
		{
			// 真横だと手前の柱と重なるので、斜め前から見ます。
			norenCamera->Place({ 2.7f, norenGround + 1.7f, ClassroomNoren::PositionZ - 1.9f },
				{ 0.0f, norenGround + 1.35f, ClassroomNoren::PositionZ + 0.2f });
		};
		// 風の揺れと押された揺れを分けて測るため、この検証では無風にします。
		cloth->SetWind({}, 0.0f);
		const float startZ = ClassroomNoren::PositionZ - 1.2f;
		walker->SetPosition({ 0.0f, MeshField::GetHeight(0.0f, startZ), startZ });

		auto step = [&](int count)
		{
			for (int i = 0; i < count; ++i)
			{
				Engine::GameTime::Update(1.0f / 60.0f);
				Engine::Manager::StepWorld(norenWorld);
				keycopy();
			}
		};
		auto draw = [&]()
		{
			Engine::Renderer::Begin();
			RenderQueue norenQueue;
			norenQueue.Build(norenWorld, norenCamera);
			norenQueue.Execute();
			Engine::Renderer::End();
		};
		auto snapshot = [&]()
		{
			std::vector<Vector3> positions;
			for (const auto& particle : cloth->GetSimulation().GetParticles())
				positions.push_back(particle.Position);
			return positions;
		};
		auto maxDifference = [&](const std::vector<Vector3>& reference)
		{
			float result = 0.0f;
			const auto& particles = cloth->GetSimulation().GetParticles();
			for (std::size_t i = 0; i < particles.size(); ++i)
				result = (std::max)(result, (particles[i].Position - reference[i]).length());
			return result;
		};

		step(180);
		const std::vector<Vector3> settled = snapshot();
		viewFromBehind();
		draw();
		norenRestCaptured = capture(L"classroom-noren-rest.png");
		viewFromSide();
		draw();
		norenSideCaptured = capture(L"classroom-noren-side.png");

		std::vector<Collider*> colliders;
		norenWorld.GetCollisionWorld().QueryBounds({ -100.0f, -100.0f, -100.0f }, { 100.0f, 100.0f, 100.0f },
			CollisionLayers::All, colliders);
		const Collider* body = colliders.empty() ? nullptr : colliders.front();
		const float surface = body ? body->GetRadius() + cloth->GetSimulation().GetSettings().CollisionThickness : 0.0f;

		Keyboard_ProcessMessage(WM_KEYDOWN, 'W', 0);
		for (int frame = 0; frame < 36; ++frame)
		{
			step(1);
			if (!body) continue;
			Vector3 start, end;
			body->GetCapsuleSegment(start, end);
			const Vector3 axis = end - start;
			for (const auto& particle : cloth->GetSimulation().GetParticles())
			{
				if (particle.Pinned) continue;
				const float t = std::clamp(Vector3::dot(particle.Position - start, axis)
					/ (std::max)(1e-6f, Vector3::dot(axis, axis)), 0.0f, 1.0f);
				const float gap = (particle.Position - (start + axis * t)).length() - surface;
				norenWorstGap = (std::min)(norenWorstGap, gap);
			}
			norenPushed = (std::max)(norenPushed, maxDifference(settled));
			// 体の前面が布に触れた頃と、体の中心が布の面を越えた直後を横から撮ります。
			const float walkerZ = walker->GetPosition().z;
			if (!norenEnterCaptured && walkerZ >= ClassroomNoren::PositionZ - 0.15f)
			{
				draw();
				norenEnterCaptured = capture(L"classroom-noren-enter.png");
			}
			if (!norenContactCaptured && walkerZ >= ClassroomNoren::PositionZ + 0.25f)
			{
				draw();
				norenContactCaptured = capture(L"classroom-noren-contact.png");
			}
		}
		Keyboard_ProcessMessage(WM_KEYUP, 'W', 0);
		norenWalkerPassed = walker->GetPosition().z > ClassroomNoren::PositionZ + 0.8f;
		step(300);
		viewFromBehind();
		draw();
		norenAfterCaptured = capture(L"classroom-noren-after.png");
		norenRemaining = maxDifference(settled);
		norenFinite = cloth->GetSimulation().IsFinite();
		norenPenetrationFree = body != nullptr && norenWorstGap >= -0.005f;
	}
	const bool norenValid = norenRestCaptured && norenSideCaptured && norenEnterCaptured
		&& norenContactCaptured && norenAfterCaptured
		&& norenFinite && norenPenetrationFree && norenWalkerPassed
		&& norenPushed > 0.15f && norenRemaining < 0.05f;

	const bool feetPlanted = flatMeasured && hillMeasured && sideMeasured
		&& std::abs(flatClearance[0]) < 0.035f && std::abs(flatClearance[1]) < 0.035f
		&& std::abs(hillClearance[0]) < 0.035f && std::abs(hillClearance[1]) < 0.035f
		&& std::abs(sideClearance[0]) < 0.035f && std::abs(sideClearance[1]) < 0.035f;
	log << "idle: " << idleCaptured << "\njump rise: " << jumpRiseCaptured
		<< "\njump peak: " << jumpPeakCaptured << "\njump landed: " << jumpLandedCaptured
		<< "\nrun start: " << startCaptured
		<< "\ngrass wind frame: " << windCaptured
		<< "\nblend to run: " << blendToRunCaptured << "\nrun: " << walkingCaptured
		<< "\nblend to idle: " << blendToIdleCaptured << "\nstopped: " << stoppedCaptured
		<< "\nslope: " << slopeCaptured << "\nslope stopped: " << slopeStoppedCaptured
		<< "\nterrain rise: " << terrainHasSlope
		<< "\nsteep hill: " << steepHill
		<< "\nlake floor below water: " << lakeFloorBelowWater
		<< "\nlake bank above water: " << lakeBankAboveWater
		<< "\nflat foot clearance: " << flatClearance[0] << ", " << flatClearance[1]
		<< "\nsteep foot clearance: " << hillClearance[0] << ", " << hillClearance[1]
		<< "\nside foot clearance: " << sideClearance[0] << ", " << sideClearance[1]
		<< "\nslope foot ends: " << slopeEndClearance[0][0] << ", " << slopeEndClearance[0][1]
		<< "; " << slopeEndClearance[1][0] << ", " << slopeEndClearance[1][1]
		<< "\nslope ends measured: " << slopeEndsMeasured
		<< "\nslope ends planted: " << slopeEndsPlanted
		<< "\nidle slope captured: " << idleSlopeCaptured
		<< "\nshader switch changed bytes: " << changedBytes
		<< "\nregistered shader changed bytes: " << registeredChangedBytes
		<< "\nanimated shader changed bytes: " << animatedChangedBytes
		<< "\nanimated shader visible: " << animatedRegisteredVisible
		<< "\nregistered post changed bytes: " << postDefinitionChangedBytes
		<< "\nregistered post visible: " << postDefinitionVisible
		<< "\nregistered posts visible: " << visibleRegisteredPostCount << "/" << registeredPostCount
		<< "\nregistered shader visible: " << registeredVisible
		<< "\nregistered model changed bytes: " << modelRegisteredChangedBytes
		<< "\nregistered model visible: " << modelRegisteredVisible
		<< "\nnamed albedo visible: " << namedAlbedoVisible
		<< "\nregistered surfaces visible: " << visibleRegisteredSurfaceCount << "/" << registeredSurfaceCount
		<< "\nnamed texture changed bytes: " << namedTextureChangedBytes
		<< "\nnamed texture visible: " << namedTextureVisible
		<< "\nshader switch visible: " << shaderVisible
		<< "\nlighting modes visible: " << workingLightingModes << "/" << lightingModes.size()
		<< "\npost effect changed bytes: " << postChangedBytes
		<< "\npost effect visible: " << postVisible
		<< "\nmosaic changed bytes: " << mosaicChangedBytes
		<< "\nmosaic visible: " << mosaicVisible
		<< "\nmip preview visible: " << mipVisible
		<< "\nbump visible: " << bumpVisible
		<< "\ntoon ramp visible: " << rampVisible
		<< "\noutline changed bytes: " << outlineChangedBytes
		<< "\noutline visible: " << outlineVisible
		<< "\nbank soles planted: " << bankSolesPlanted
		<< "\nworst bank sole gap: " << worstSlopeEndGap
		<< "\nfeet planted: " << feetPlanted
		<< "\nleg chains: " << legChainsValid
		<< "\nnoren pushed by player: " << norenPushed
		<< "\nnoren worst gap to body: " << norenWorstGap
		<< "\nnoren remaining after pass: " << norenRemaining
		<< "\nnoren walker passed: " << norenWalkerPassed
		<< "\nnoren finite: " << norenFinite
		<< "\nnoren captured: " << norenRestCaptured << norenSideCaptured << norenEnterCaptured
		<< norenContactCaptured << norenAfterCaptured
		<< "\nnoren valid: " << norenValid << '\n';
	return transitionsPassed == 30 && interruptionsPassed == 30 && loopsPassed == 5 && jumpHolds
		&& playerStatesPassed == 13
		&& swimDepthValid && treadDepthValid && swimmingDepthCaptured && treadingDepthCaptured
		&& shoreExitValid
		&& idleCaptured && windCaptured && startCaptured && blendToRunCaptured && walkingCaptured
		&& blendToIdleCaptured && stoppedCaptured && slopeCaptured && slopeStoppedCaptured
		&& terrainHasSlope && steepHill && lakeFloorBelowWater && lakeBankAboveWater
		&& feetPlanted && slopeEndsPlanted && bankSolesPlanted
		&& idleSlopeCaptured && shaderVisible && registeredVisible && modelRegisteredVisible
		&& animatedRegisteredVisible
		&& postDefinitionVisible && allRegisteredPostsVisible && allRegisteredSurfacesVisible
		&& namedTextureVisible && namedAlbedoVisible
		&& lightingModesVisible
		&& postVisible && mosaicVisible && mipVisible
		&& bumpVisible && rampVisible && outlineVisible && legChainsValid
		&& norenValid;
}
