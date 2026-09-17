#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "DirectXTex.h"
#include "classroomWalk.h"
#include "keyboard_utf8.h"
#include "manager.h"
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
	ClassroomCamera* slopeCamera = contactWorld.Spawn<ClassroomCamera>();
	slopeCamera->LateUpdate();
	Renderer::Begin();
	RenderQueue slopeQueue;
	slopeQueue.Build(contactWorld, slopeCamera);
	slopeQueue.Execute();
	Renderer::End();
	const bool idleSlopeCaptured = capture(L"classroom-idle-slope.png");
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
		<< "\nbank soles planted: " << bankSolesPlanted
		<< "\nworst bank sole gap: " << worstSlopeEndGap
		<< "\nfeet planted: " << feetPlanted
		<< "\nleg chains: " << legChainsValid << '\n';
	return idleCaptured && windCaptured && startCaptured && blendToRunCaptured && walkingCaptured
		&& blendToIdleCaptured && stoppedCaptured && slopeCaptured && slopeStoppedCaptured
		&& terrainHasSlope && steepHill && lakeFloorBelowWater && lakeBankAboveWater
		&& feetPlanted && slopeEndsPlanted && bankSolesPlanted
		&& idleSlopeCaptured && legChainsValid;
}
