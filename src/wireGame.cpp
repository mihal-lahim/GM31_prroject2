#include "wireGame.h"

#include "gameTime.h"
#include "keyboard_utf8.h"
#include "particleSystem.h"
#include "score.h"
#include "wireAnchor.h"
#include "wireEnemy.h"
#include "wirePlatform.h"
#include "wirePlayer.h"
#include "wireReticle.h"
#include "wireStage.h"
#include "wireSword.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"

#include <algorithm>
#include <cmath>

namespace
{
	struct PlatformPlacement
	{
		Vector3 Position;
		Vector3 Scale;
	};

	struct AnchorPlacement
	{
		Vector3 Position;
		Vector3 Scale;
	};

	struct SceneryBuildingPlacement
	{
		float X;
		float Z;
		Vector3 Scale;
	};

	// コースは手前（Z=0）から奥（Z=237）へ進みます。
	// 足場は開始・中継3か所・ゴールだけに限定し、その間をワイヤーで渡る設計です。
	constexpr float kReferenceCourseLength = 237.0f;
	const PlatformPlacement kPlatformPlacements[] =
	{
		{ {  0.0f, -2.0f,   0.0f }, { 9.0f, 1.0f, 8.0f } },
		{ { -7.0f,  2.0f,  58.0f }, { 6.0f, 1.0f, 5.0f } },
		{ {  8.0f,  8.0f, 116.0f }, { 5.5f, 1.0f, 5.0f } },
		{ { -6.0f, 12.0f, 176.0f }, { 5.5f, 1.0f, 5.0f } },
		{ {  0.0f, 16.0f, 237.0f }, { 9.0f, 1.0f, 8.0f } }
	};

	// 射程42m以内で次のアンカーが見つかるよう、18～24m間隔で左右と高さにリズムを付けています。
	const AnchorPlacement kAnchorPlacements[] =
	{
		{ {  0.0f, 14.0f,  18.0f }, { 1.30f, 1.30f, 1.30f } },
		{ { -8.0f, 18.0f,  38.0f }, { 1.35f, 1.35f, 1.35f } },
		{ { -2.0f, 17.0f,  54.0f }, { 1.35f, 1.35f, 1.35f } },
		{ {  7.0f, 22.0f,  76.0f }, { 1.40f, 1.40f, 1.40f } },
		{ { -6.0f, 28.0f,  98.0f }, { 1.45f, 1.45f, 1.45f } },
		{ {  7.0f, 25.0f, 112.0f }, { 1.45f, 1.45f, 1.45f } },
		{ {  0.0f, 32.0f, 134.0f }, { 1.50f, 1.50f, 1.50f } },
		{ { -10.0f, 38.0f, 154.0f }, { 1.55f, 1.55f, 1.55f } },
		{ { -4.0f, 34.0f, 172.0f }, { 1.55f, 1.55f, 1.55f } },
		{ {  8.0f, 41.0f, 194.0f }, { 1.60f, 1.60f, 1.60f } },
		{ { -7.0f, 43.0f, 214.0f }, { 1.65f, 1.65f, 1.65f } },
		{ {  0.0f, 36.0f, 232.0f }, { 2.00f, 2.00f, 2.00f } }
	};

	const Vector3 kEnemyPlacements[] =
	{
		{ -7.0f,  4.0f,  58.0f },
		{  8.0f, 10.0f, 116.0f },
		{ -6.0f, 14.0f, 176.0f },
		{  0.0f, 18.0f, 237.0f }
	};

	// コースの両側へ交互の高さで並べ、奥へ進んでいることが分かる背景シルエットを作ります。
	// WirePlatformの箱モデルを共有しますが、着地判定は無効にします。
	const SceneryBuildingPlacement kSceneryBuildingPlacements[] =
	{
		{ -42.0f,   2.0f, { 8.0f, 16.0f, 10.0f } },
		{  43.0f,   8.0f, { 9.0f, 22.0f, 11.0f } },
		{ -45.0f,  32.0f, { 9.0f, 24.0f, 10.0f } },
		{  40.0f,  39.0f, { 7.0f, 17.0f, 12.0f } },
		{ -40.0f,  63.0f, { 7.0f, 29.0f, 11.0f } },
		{  45.0f,  69.0f, { 9.0f, 21.0f, 10.0f } },
		{ -44.0f,  94.0f, { 10.0f, 20.0f, 12.0f } },
		{  41.0f, 100.0f, { 7.0f, 31.0f, 10.0f } },
		{ -41.0f, 126.0f, { 8.0f, 25.0f, 11.0f } },
		{  45.0f, 132.0f, { 9.0f, 19.0f, 12.0f } },
		{ -45.0f, 158.0f, { 9.0f, 32.0f, 10.0f } },
		{  40.0f, 164.0f, { 7.0f, 24.0f, 11.0f } },
		{ -40.0f, 190.0f, { 7.0f, 22.0f, 12.0f } },
		{  44.0f, 196.0f, { 9.0f, 34.0f, 10.0f } },
		{ -44.0f, 222.0f, { 10.0f, 28.0f, 11.0f } },
		{  41.0f, 230.0f, { 8.0f, 23.0f, 12.0f } }
	};

	constexpr int kScoreDigits = 3;
	const Vector3 kScorePosition{ 180.0f, 20.0f, 0.0f };

	Vector3 ToWorldPosition(const Vector3& localPosition)
	{
		const WireStageTuning& stage = GetWireGameplayTuning().Stage;
		const float zScale = stage.FinishOffsetZ / kReferenceCourseLength;
		return {
			stage.OriginX + localPosition.x,
			stage.OriginY + localPosition.y,
			stage.OriginZ + localPosition.z * zScale
		};
	}

	float PlatformTopY(const PlatformPlacement& placement)
	{
		return ToWorldPosition(placement.Position).y + placement.Scale.y * 2.0f;
	}

	void AddAnchor(World& world, const Vector3& position, const Vector3& scale)
	{
		WireAnchor* anchor = world.Spawn<WireAnchor>();
		anchor->SetPosition(position);
		anchor->SetScale(scale);
	}

	void AddPlatform(World& world, const Vector3& position, const Vector3& scale)
	{
		WirePlatform* platform = world.Spawn<WirePlatform>();
		platform->SetPosition(position);
		platform->SetScale(scale);
	}

	void AddSceneryBuilding(World& world, const SceneryBuildingPlacement& placement)
	{
		const WireStageTuning& stage = GetWireGameplayTuning().Stage;
		WirePlatform* building = world.Spawn<WirePlatform>();
		building->SetPosition(ToWorldPosition({ placement.X, -stage.FallDepth - 3.0f, placement.Z }));
		building->SetScale(placement.Scale);
		building->SetLandingEnabled(false);
	}

	void AddEnemy(World& world, const Vector3& position)
	{
		WireEnemy* enemy = world.Spawn<WireEnemy>();
		enemy->SetPosition(position);
	}

	void EmitGoalCelebration(const Vector3& center)
	{
		const int count = GetWireGameplayTuning().Stage.GoalParticleCount;
		for (int i = 0; i < count; ++i)
		{
			const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>((std::max)(count, 1));
			const float speed = 5.5f + static_cast<float>(i % 7) * 0.75f;
			Vector3 velocity{ cosf(angle) * speed, 6.0f + static_cast<float>(i % 5), sinf(angle) * speed };
			const XMFLOAT4 color = (i % 3 == 0)
				? XMFLOAT4(1.0f, 0.90f, 0.30f, 1.0f)
				: XMFLOAT4(0.20f, 0.90f, 1.0f, 0.95f);
			ParticleSystem::Emit(center + Vector3(0.0f, 2.0f, 0.0f), velocity, 0.22f, 52, color, -7.0f, 0.975f);
		}
	}
}

void WireGame::Init()
{
	GameTime::SetPaused(false);
	World& world = GetWorld();

	WirePlayer* player = world.Spawn<WirePlayer>();
	world.Spawn<WireSword>();
	world.Spawn<WireTpsCamera>();
	world.Spawn<WireStage>();
	world.Spawn<ParticleSystem>();

	for (const PlatformPlacement& placement : kPlatformPlacements)
	{
		AddPlatform(world, ToWorldPosition(placement.Position), placement.Scale);
	}

	for (const SceneryBuildingPlacement& placement : kSceneryBuildingPlacements)
	{
		AddSceneryBuilding(world, placement);
	}

	for (const AnchorPlacement& placement : kAnchorPlacements)
	{
		AddAnchor(world, ToWorldPosition(placement.Position), placement.Scale);
	}

	for (const Vector3& position : kEnemyPlacements)
	{
		AddEnemy(world, ToWorldPosition(position));
	}

	world.Spawn<WireReticle>();
	m_Score = world.Spawn<Score>();
	m_Score->SetDigitCount(kScoreDigits);
	m_Score->SetPosition(kScorePosition);

	// 各足場をチェックポイントとして扱い、落下時は最後に着地した場所へ戻します。
	m_RespawnPosition = player->GetPosition();
	m_UnlockedCheckpointIndex = 0;
	m_BestProgressPercent = 0;
	m_Completed = false;
}

void WireGame::Uninit()
{
}

void WireGame::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_TAB))
	{
		GameTime::SetPaused(!GameTime::IsPaused());
	}

	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player == nullptr)
	{
		return;
	}

	const WireStageTuning& stage = GetWireGameplayTuning().Stage;
	if (player->GetPosition().y < stage.OriginY - stage.FallDepth)
	{
		player->Respawn(m_RespawnPosition);
	}

	// 中継足場へ実際に着地したときだけ復帰地点を更新します。
	for (int index = m_UnlockedCheckpointIndex + 1;
		index < static_cast<int>(std::size(kPlatformPlacements)); ++index)
	{
		const PlatformPlacement& checkpoint = kPlatformPlacements[index];
		const Vector3 checkpointPosition = ToWorldPosition(checkpoint.Position);
		const Vector3 playerPosition = player->GetPosition();
		const bool inside = std::abs(playerPosition.x - checkpointPosition.x) <= checkpoint.Scale.x + 0.5f
			&& std::abs(playerPosition.z - checkpointPosition.z) <= checkpoint.Scale.z + 0.5f;
		const bool atTop = std::abs(playerPosition.y - PlatformTopY(checkpoint))
			<= stage.CheckpointHeightTolerance;
		if (!player->IsOnGround() || !inside || !atTop)
		{
			continue;
		}

		m_UnlockedCheckpointIndex = index;
		m_RespawnPosition = { checkpointPosition.x, PlatformTopY(checkpoint), checkpointPosition.z };
		break;
	}

	const float progress = std::clamp(
		(player->GetPosition().z - stage.OriginZ) / stage.FinishOffsetZ, 0.0f, 1.0f);
	const int progressPercent = static_cast<int>(progress * 100.0f);
	if (progressPercent > m_BestProgressPercent && m_Score != nullptr)
	{
		m_Score->Add(progressPercent - m_BestProgressPercent);
		m_BestProgressPercent = progressPercent;
	}

	const int lastCheckpointIndex = static_cast<int>(std::size(kPlatformPlacements)) - 1;
	if (!m_Completed && m_UnlockedCheckpointIndex == lastCheckpointIndex)
	{
		m_Completed = true;
		if (m_Score != nullptr && m_BestProgressPercent < 100)
		{
			m_Score->Add(100 - m_BestProgressPercent);
			m_BestProgressPercent = 100;
		}
		EmitGoalCelebration(player->GetPosition());
	}
}

void WireGame::Draw()
{
}
