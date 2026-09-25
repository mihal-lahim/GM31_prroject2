#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "DirectXTex.h"
#include "classroomWalk.h"
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
			Manager::Update(1.0f / 60.0f);
			keycopy();
			Manager::Draw();
		}
	};
	auto capture = [](const wchar_t* path)
	{
		ComPtr<ID3D11RenderTargetView> target;
		Renderer::GetDeviceContext()->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		if (!target) return false;
		ComPtr<ID3D11Resource> resource;
		target->GetResource(resource.GetAddressOf());
		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(Renderer::GetDevice(),
			Renderer::GetDeviceContext(), resource.Get(), image))) return false;
		return SUCCEEDED(DirectX::SaveToWICFile(*image.GetImage(0, 0, 0),
			DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path));
	};

	frames(60);
	const bool idleCaptured = capture(L"classroom-idle.png");
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
	const bool terrainHasSlope = ClassroomField::GetHeight(-12.0f, 13.0f)
		> ClassroomField::GetHeight(0.0f, 0.0f) + 0.5f;
	bool steepHill = false;
	for (int z = 18; z <= 20; ++z)
		steepHill |= ClassroomField::GetNormal(0.0f, static_cast<float>(z) + 0.25f).y < 0.82f;
	const bool lakeFloorBelowWater = ClassroomField::GetHeight(ClassroomLake::CenterX, ClassroomLake::CenterZ)
		< ClassroomLake::WaterLevel - 1.0f;
	const bool lakeBankAboveWater = ClassroomField::GetHeight(ClassroomLake::CenterX + ClassroomLake::WaterRadius,
		ClassroomLake::CenterZ) > ClassroomLake::WaterLevel;
	World contactWorld;
	ClassroomPlayer* contactPlayer = contactWorld.Spawn<ClassroomPlayer>();
	float flatClearance[2]{}, hillClearance[2]{}, sideClearance[2]{};
	contactPlayer->Update();
	const bool flatMeasured = contactPlayer->GetFootClearance(flatClearance);
	contactPlayer->SetPosition({ 0.0f, 0.0f, 10.5f });
	contactPlayer->Update();
	const bool hillMeasured = contactPlayer->GetFootClearance(hillClearance);
	contactPlayer->SetRotation({ 0.0f, -XM_PIDIV4, 0.0f });
	contactPlayer->SetPosition({ -8.0f, 0.0f, 11.0f });
	contactPlayer->Update();
	const bool sideMeasured = contactPlayer->GetFootClearance(sideClearance);
	contactPlayer->SetRotation({ 0.0f, 0.0f, 0.0f });
	contactPlayer->SetPosition({ 0.0f, 0.0f, 19.0f });
	contactPlayer->Update();
	float slopeEndClearance[2][2]{};
	const bool slopeEndsMeasured = contactPlayer->GetFootEndClearance(slopeEndClearance);
	bool slopeEndsPlanted = slopeEndsMeasured
		&& ClassroomField::GetNormal(0.0f, 19.0f).y < 0.85f;
	for (const auto& foot : slopeEndClearance)
		for (float gap : foot)
			slopeEndsPlanted &= std::abs(gap) < 0.035f;
	contactWorld.Spawn<ClassroomField>();
	ClassroomField* comparisonField = contactWorld.FindFirst<ClassroomField>();
	ClassroomCamera* slopeCamera = contactWorld.Spawn<ClassroomCamera>();
	slopeCamera->LateUpdate();
	Renderer::Begin();
	RenderQueue slopeQueue;
	slopeQueue.Build(contactWorld, slopeCamera);
	slopeQueue.Execute();
	Renderer::End();
	const bool idleSlopeCaptured = capture(L"classroom-idle-slope.png");
	// 同じ地形とカメラを描き直し、描画対象のシェーダー切替が画素に効くことを検証する。
	auto terrainPixels = [&]()
	{
		Renderer::Begin();
		slopeQueue.Build(contactWorld, slopeCamera);
		slopeQueue.Execute();
		Renderer::End();
		ComPtr<ID3D11RenderTargetView> target;
		Renderer::GetDeviceContext()->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		if (!target) return std::vector<BYTE>{};
		ComPtr<ID3D11Resource> resource;
		target->GetResource(resource.GetAddressOf());
		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(Renderer::GetDevice(),
			Renderer::GetDeviceContext(), resource.Get(), image))) return std::vector<BYTE>{};
		const DirectX::Image* frame = image.GetImage(0, 0, 0);
		return std::vector<BYTE>(frame->pixels, frame->pixels + frame->slicePitch);
	};
	const std::vector<BYTE> toonPixels = terrainPixels();
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
	const std::size_t animatedChangedBytes = difference(toonPixels, animatedRegisteredPixels);
	const bool animatedRegisteredVisible = animatedDefinitionLoaded && animatedRegisteredCaptured
		&& animatedChangedBytes > 1000;
	contactPlayer->UseDefaultShader();
	const bool postDefinitionLoaded = Renderer::SetPostEffectDefinition("post-invert");
	const std::vector<BYTE> postDefinitionPixels = terrainPixels();
	const bool postDefinitionCaptured = capture(L"classroom-post-invert.png");
	const bool postParameterChanged = Renderer::SetPostEffectFloat("strength", 0.0f);
	const std::vector<BYTE> postParameterPixels = terrainPixels();
	const std::size_t postDefinitionChangedBytes = difference(toonPixels, postDefinitionPixels);
	const bool postDefinitionVisible = postDefinitionLoaded && postDefinitionCaptured
		&& postParameterChanged && postDefinitionChangedBytes > 50000
		&& difference(postDefinitionPixels, postParameterPixels) > 50000;
	Renderer::SetPostEffect(PostEffectMode::None);
	int registeredPostCount = 0;
	int visibleRegisteredPostCount = 0;
	for (const Engine::ShaderDefinition& definition : Engine::ShaderCatalog)
	{
		if (definition.Usage != Engine::ShaderUsage::Post) continue;
		++registeredPostCount;
		if (Renderer::SetPostEffectDefinition(definition.Id)
			&& difference(toonPixels, terrainPixels()) > 50000)
			++visibleRegisteredPostCount;
		Renderer::SetPostEffect(PostEffectMode::None);
	}
	const bool allRegisteredPostsVisible = registeredPostCount > 0
		&& visibleRegisteredPostCount == registeredPostCount;
	const bool registeredLoaded = comparisonField->SetShaderDefinition("sample-stripes");
	const std::vector<BYTE> registeredPixels = terrainPixels();
	const bool registeredCaptured = capture(L"classroom-sample-stripes.png");
	const std::size_t registeredChangedBytes = difference(toonPixels, registeredPixels);
	const bool registeredVisible = registeredLoaded && registeredCaptured
		&& registeredChangedBytes > 50000;
	comparisonField->SetShaderMode(Engine::SurfaceShader::Toon);
	Renderer::SetPostEffect(PostEffectMode::Grayscale);
	const std::vector<BYTE> postPixels = terrainPixels();
	const bool postCaptured = capture(L"classroom-post-grayscale.png");
	Renderer::SetPostEffect(PostEffectMode::Mosaic);
	const std::vector<BYTE> mosaicPixels = terrainPixels();
	const bool mosaicCaptured = capture(L"classroom-post-mosaic.png");
	Renderer::SetPostEffect(PostEffectMode::MipPreview);
	const std::vector<BYTE> mipPixels = terrainPixels();
	const bool mipCaptured = capture(L"classroom-post-mip.png");
	Renderer::SetPostEffect(PostEffectMode::None);
	const std::size_t postChangedBytes = difference(toonPixels, postPixels);
	const bool postVisible = postCaptured && postChangedBytes > 50000;
	const std::size_t mosaicChangedBytes = difference(toonPixels, mosaicPixels);
	const bool mosaicVisible = mosaicCaptured && mosaicChangedBytes > 50000;
	const bool mipVisible = mipCaptured && difference(toonPixels, mipPixels) > 50000;
	const bool normalMapLoaded = ResourceManager::LoadTexture(L"assets/texture/Normal.png") != nullptr;
	const bool bumpSelected = comparisonField->SetShaderMode(Engine::SurfaceShader::Bump);
	const std::vector<BYTE> bumpPixels = terrainPixels();
	const bool bumpCaptured = capture(L"classroom-bump.png");
	const bool bumpVisible = normalMapLoaded && bumpSelected && bumpCaptured
		&& difference(toonPixels, bumpPixels) > 50000;
	const bool rampLoaded = ResourceManager::LoadTexture(L"assets/texture/Toon2.png") != nullptr;
	const bool rampSelected = comparisonField->SetShaderMode(Engine::SurfaceShader::ToonRamp);
	const std::vector<BYTE> rampPixels = terrainPixels();
	const bool rampCaptured = capture(L"classroom-toon-ramp.png");
	const bool rampVisible = rampLoaded && rampSelected && rampCaptured
		&& difference(toonPixels, rampPixels) > 50000;
	const bool shaderChanged = comparisonField->SetShaderMode(Engine::SurfaceShader::Unlit);
	const std::vector<BYTE> unlitPixels = terrainPixels();
	const std::size_t changedBytes = difference(toonPixels, unlitPixels);
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
	comparisonField->SetShaderMode(Engine::SurfaceShader::Toon);
	GameObject* outlineObject = contactWorld.Spawn<GameObject>();
	outlineObject->SetPosition({ 3.0f, ClassroomField::GetHeight(3.0f, 23.0f) + 0.42f, 23.0f });
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
		contactPlayer->SetPosition({ sample.x, 0.0f, sample.z });
		contactPlayer->Update();
		float ends[2][2]{};
		bankSolesPlanted &= contactPlayer->GetFootEndClearance(ends);
		for (const auto& foot : ends)
			for (float gap : foot)
				worstSlopeEndGap = (std::max)(worstSlopeEndGap, std::abs(gap));
	}
	bankSolesPlanted &= worstSlopeEndGap < 0.035f;
	const bool feetPlanted = flatMeasured && hillMeasured && sideMeasured
		&& std::abs(flatClearance[0]) < 0.035f && std::abs(flatClearance[1]) < 0.035f
		&& std::abs(hillClearance[0]) < 0.035f && std::abs(hillClearance[1]) < 0.035f
		&& std::abs(sideClearance[0]) < 0.035f && std::abs(sideClearance[1]) < 0.035f;
	log << "idle: " << idleCaptured << "\nrun start: " << startCaptured
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
		<< "\nleg chains: " << legChainsValid << '\n';
	return idleCaptured && windCaptured && startCaptured && blendToRunCaptured && walkingCaptured
		&& blendToIdleCaptured && stoppedCaptured && slopeCaptured && slopeStoppedCaptured
		&& terrainHasSlope && steepHill && lakeFloorBelowWater && lakeBankAboveWater
		&& feetPlanted && slopeEndsPlanted && bankSolesPlanted
		&& idleSlopeCaptured && shaderVisible && registeredVisible && modelRegisteredVisible
		&& animatedRegisteredVisible
		&& postDefinitionVisible && allRegisteredPostsVisible && allRegisteredSurfacesVisible
		&& namedTextureVisible && namedAlbedoVisible
		&& lightingModesVisible
		&& postVisible && mosaicVisible && mipVisible
		&& bumpVisible && rampVisible && outlineVisible && legChainsValid;
}
