#include "slowAction.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "collider.h"
#include "modelRenderer.h"
#include "dynamicText.h"
#include "gameTime.h"
#include "keyboard_utf8.h"
#include "mouseInput.h"
#include "particleSystem.h"
#include "propertyVisitor.h"
#include "sceneEditor.h"
#include "sceneSerializer.h"
#include "sky.h"

#include <cstdarg>

namespace
{
	constexpr const char* kStagePath = "assets/scene/slow-stage.scene";
	constexpr const wchar_t* kPlayHelp =
		L"WASD:移動  マウス:視点  Shift:ダッシュ  左クリック:短押し=通常 / 長押し=強攻撃  "
		L"右クリック:時間減速  R:やり直し  F1:当たり判定  F2:敵無敵  F3:編集モード  Esc:終了";
	// 現在のステージの記述(シーンファイルと同じ書式)です。最初はファイルから読み、
	// 編集モードを抜けるたびにその時点の配置で置き換えます。Sceneを作り直しても残るよう、ここで持ちます。
	std::string g_StageText;

	SlowInputState g_Input;
	const SlowInputState* g_InputOverride = nullptr;

	// 時間減速の状態です。シーンの初期化でResetします。
	float g_WorldScale = 1.0f;
	float g_WorldDeltaTime = 0.0f;
	float g_Gauge = 0.0f;
	float g_PendingGain = 0.0f;
	bool g_SlowActive = false;
	// ゲージ切れで強制解除された後、右クリックを一度離すまで再発動させないためのフラグです。
	bool g_SlowLocked = false;

	void ApplyPendingGain()
	{
		g_Gauge = (std::min)(GetSlowActionTuning().GaugeMax, g_Gauge + g_PendingGain);
		g_PendingGain = 0.0f;
	}

	std::wstring Format(const wchar_t* format, ...)
	{
		wchar_t text[256]{};
		va_list arguments;
		va_start(arguments, format);
		vswprintf_s(text, format, arguments);
		va_end(arguments);
		return text;
	}

	const wchar_t* AttackName(SlowPlayer::AttackType type)
	{
		switch (type)
		{
		case SlowPlayer::AttackType::Normal: return L"通常";
		case SlowPlayer::AttackType::Strong: return L"強";
		default: return L"なし";
		}
	}

	const wchar_t* EnemyStateName(SlowEnemy::State state)
	{
		switch (state)
		{
		case SlowEnemy::State::Idle: return L"待機";
		case SlowEnemy::State::Chase: return L"接近";
		case SlowEnemy::State::Windup: return L"攻撃予兆";
		case SlowEnemy::State::Cooldown: return L"攻撃後の硬直";
		case SlowEnemy::State::Stagger: return L"ひるみ";
		case SlowEnemy::State::Launched: return L"吹き飛び";
		case SlowEnemy::State::Recover: return L"受け身";
		default: return L"?";
		}
	}
}

void SlowInput::Poll()
{
	if (g_InputOverride != nullptr)
	{
		g_Input = *g_InputOverride;
		return;
	}
	SlowInputState state;
	if (Keyboard_IsKeyDown(KK_W)) state.MoveZ += 1.0f;
	if (Keyboard_IsKeyDown(KK_S)) state.MoveZ -= 1.0f;
	if (Keyboard_IsKeyDown(KK_D)) state.MoveX += 1.0f;
	if (Keyboard_IsKeyDown(KK_A)) state.MoveX -= 1.0f;
	state.Dash = Keyboard_IsKeyDown(KK_LEFTSHIFT);
	state.Attack = Mouse_IsLeftButtonDown();
	state.Slow = Mouse_IsRightButtonDown();
	g_Input = state;
}

const SlowInputState& SlowInput::Get()
{
	return g_Input;
}

void SlowInput::SetOverride(const SlowInputState* state)
{
	g_InputOverride = state;
}

void SlowClock::Reset()
{
	g_WorldScale = 1.0f;
	g_WorldDeltaTime = 0.0f;
	g_Gauge = GetSlowActionTuning().GaugeMax;
	g_PendingGain = 0.0f;
	g_SlowActive = false;
	g_SlowLocked = false;
}

void SlowClock::Update(float realDeltaTime, bool slowHeld)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	if (!slowHeld)
	{
		g_SlowLocked = false;
	}

	const bool wasActive = g_SlowActive;
	g_SlowActive = slowHeld && !g_SlowLocked && g_Gauge > 0.0f;
	if (g_SlowActive)
	{
		// ゲージは実時間で減ります。自然回復はしません。
		g_Gauge -= tuning.GaugeDrainPerSecond * realDeltaTime;
		if (g_Gauge <= 0.0f)
		{
			g_Gauge = 0.0f;
			g_SlowActive = false;
			g_SlowLocked = true;
		}
	}
	if (wasActive && !g_SlowActive)
	{
		// 減速中にためた回復量は、減速が終わった瞬間にまとめて反映します。
		ApplyPendingGain();
	}

	// 倍率は短い時間で補間し、切り替わりの瞬間に敵が飛んで見えないようにします。
	const float target = g_SlowActive ? tuning.SlowTimeScale : 1.0f;
	const float maxStep = (1.0f - tuning.SlowTimeScale) / tuning.SlowBlendSeconds * realDeltaTime;
	g_WorldScale += std::clamp(target - g_WorldScale, -maxStep, maxStep);
	g_WorldDeltaTime = realDeltaTime * g_WorldScale;
}

float SlowClock::GetWorldScale() { return g_WorldScale; }
float SlowClock::GetWorldDeltaTime() { return g_WorldDeltaTime; }
bool SlowClock::IsSlowActive() { return g_SlowActive; }
float SlowClock::GetGauge() { return g_Gauge; }
float SlowClock::GetPendingGain() { return g_PendingGain; }

void SlowClock::AddGauge(float amount)
{
	if (g_SlowActive)
	{
		g_PendingGain += amount;
	}
	else
	{
		g_Gauge = (std::min)(GetSlowActionTuning().GaugeMax, g_Gauge + amount);
	}
}

void SlowBlock::Init()
{
	m_Layer = Layer::World;
	// モデルは-1〜1の立方体なので、scaleがそのまま見た目と当たり判定の半サイズになります。
	// 壁用と障害物用の色違いを両方持ち、Obstacleの値で見せる方を切り替えます。
	const auto addModel = [this](const char* path)
	{
		ModelRenderer* model = AddComponent<ModelRenderer>()->SetUseOwnerTransform();
		model->Load(path);
		if (model->SetShaderDefinition("realistic"))
		{
			model->SetShaderFloat("roughness", 0.85f);
		}
		return model;
	};
	m_WallModel = addModel("assets\\model\\slowWall.obj");
	m_ObstacleModel = addModel("assets\\model\\slowObstacle.obj");
	OnPropertiesChanged();
	AddComponent<Collider>()
		->SetAABB({ 1.0f, 1.0f, 1.0f })
		->SetCollisionLayer(SlowCollisionLayers::World)
		->SetStatic();
}

void SlowBlock::Reflect(Engine::PropertyVisitor& visitor)
{
	visitor.Visit("Obstacle", m_Obstacle);
}

void SlowBlock::OnPropertiesChanged()
{
	m_WallModel->SetVisible(!m_Obstacle);
	m_ObstacleModel->SetVisible(m_Obstacle);
}

void SlowFloor::Init()
{
	m_Layer = Layer::World;
	ModelRenderer* model = AddComponent<ModelRenderer>()->SetUseOwnerTransform();
	model->Load("assets\\model\\slowFloor.obj");
	if (model->SetShaderDefinition("realistic"))
	{
		model->SetShaderFloat("roughness", 0.9f);
	}
}

void SlowActionScene::Init()
{
	Collider::SetDebugDrawEnabled(false);
	SlowEnemy::SetInvincible(false);
	SlowClock::Reset();
	World& world = GetWorld();
	// シーンファイルに書ける型です。ここへ登録した型だけが保存され、エディタから追加できます。
	Engine::ObjectTypeRegistry::Register<SlowFloor>("SlowFloor");
	Engine::ObjectTypeRegistry::Register<SlowPlayer>("SlowPlayer");
	Engine::ObjectTypeRegistry::Register<SlowBlock>("SlowBlock");
	Engine::ObjectTypeRegistry::Register<SlowEnemy>("SlowEnemy");

	// 床・壁・障害物・プレイヤー・敵の配置はコードではなくシーンファイルが持ちます。
	// 編集モードで変更した内容(g_StageText)があれば、やり直してもそれを使います。
	// ファイルに書かれたIDをそのまま使えるよう、コードで生成するものより先に読み込みます
	// (後から生成するカメラやUIには、読み込んだIDの続きの番号が振られます)。
	if (g_StageText.empty())
	{
		g_StageText = Engine::SceneSerializer::ReadFileText(kStagePath);
	}
	Engine::SceneSerializer::Read(world, g_StageText);

	// カメラの向きはPreUpdate、追従はLateUpdateなので、プレイヤーより後に生成しても順序は変わりません。
	SlowCamera* camera = world.Spawn<SlowCamera>();
	world.Spawn<Sky>()->SetRadius(90.0f);
	world.Spawn<ParticleSystem>();
	if (const SlowPlayer* player = world.FindFirst<SlowPlayer>())
	{
		camera->SetYaw(player->GetYaw());
	}
	camera->SnapToTarget();

	SceneEditor::SetActive(false);
	SceneEditor::SetWorld(&world);
	SceneEditor::SetScenePaths(kStagePath, SceneEditor::FindSourcePath(kStagePath, "SlowAction.vcxproj"));

	m_Status = world.Spawn<DynamicText>();
	m_Status->SetFontSize(22);
	m_Status->SetPosition({ 16.0f, 12.0f, 0.0f });
	m_EnemyInfo = world.Spawn<DynamicText>();
	m_EnemyInfo->SetFontSize(18);
	m_EnemyInfo->SetPosition({ 16.0f, 150.0f, 0.0f });

	m_Help = world.Spawn<DynamicText>();
	m_Help->SetFontSize(18);
	m_Help->SetColor({ 1.0f, 1.0f, 1.0f, 0.8f });
	m_Help->SetPosition({ 16.0f, SCREEN_HEIGHT - 36.0f, 0.0f });
	m_Help->SetText(kPlayHelp);
	m_RespawnTimer = 0.0f;
	UpdateHud();
}

void SlowActionScene::Uninit()
{
	SceneEditor::SetActive(false);
	SceneEditor::SetWorld(nullptr);
}

void SlowActionScene::RespawnEnemies()
{
	// ステージの記述から敵だけを生成し直します。
	Engine::SceneSerializer::Read(GetWorld(), g_StageText, nullptr,
		[](std::string_view typeName) { return typeName == "SlowEnemy"; });
}

// 編集モードと再生の切り替えです。どちらの向きでも、ステージを記述から作り直します。
//   再生→編集: 戦闘で動いた位置や減ったHPを捨て、最後に確定した配置へ戻してから編集します。
//   編集→再生: 今の配置を記述として確定し、そこから新しく始めます(Reflectを通る経路を1本にするため)。
// ファイルへ書くのはエディタの「保存」だけで、ここでは書きません。
void SlowActionScene::ToggleEditMode()
{
	World& world = GetWorld();
	const bool enterEdit = !SceneEditor::IsActive();
	if (!enterEdit)
	{
		g_StageText = Engine::SceneSerializer::Write(world);
	}
	Engine::SceneSerializer::DestroyAll(world);
	Engine::SceneSerializer::Read(world, g_StageText);
	SlowClock::Reset();
	m_RespawnTimer = 0.0f;

	SlowCamera* camera = world.FindFirst<SlowCamera>();
	if (enterEdit && camera != nullptr)
	{
		// 今見ている視点から編集を始めます。
		SceneEditor::CameraPose pose;
		pose.Position = camera->GetCameraPosition();
		pose.Yaw = camera->GetYaw();
		pose.Pitch = camera->GetPitch();
		pose.FovDegrees = GetSlowActionTuning().CameraFovDegrees;
		SceneEditor::SetCamera(pose);
	}
	else if (camera != nullptr)
	{
		camera->SnapToTarget();
	}
	SceneEditor::SetActive(enterEdit);
}

void SlowActionScene::Update()
{
	// 名前の入力中は、文字がゲームのキー操作として効かないようにします。
	if (!SceneEditor::WantsKeyboard())
	{
		if (Keyboard_IsKeyDownTrigger(KK_F3))
		{
			ToggleEditMode();
		}
		if (Keyboard_IsKeyDownTrigger(KK_F1))
		{
			Collider::SetDebugDrawEnabled(!Collider::GetDebugDrawEnabled());
		}
	}
	if (SceneEditor::IsActive())
	{
		// 編集中はゲームの時間を進めません。プレイヤーと敵も自分のUpdateで止まります。
		m_Status->SetColor({ 1.0f, 0.9f, 0.4f, 1.0f });
		m_Status->SetText(L"");
		m_EnemyInfo->SetText(L"");
		m_Help->SetText(L"編集モード  F3:再生に戻る  右ドラッグ:見回す  WASD/QE:移動(Shiftで加速)  ホイール:前後  "
			L"左クリック:選択  F1:当たり判定表示");
		return;
	}
	m_Help->SetText(kPlayHelp);

	if (Keyboard_IsKeyDownTrigger(KK_R))
	{
		ChangeScene<SlowActionScene>();
		return;
	}
	if (Keyboard_IsKeyDownTrigger(KK_F2))
	{
		SlowEnemy::SetInvincible(!SlowEnemy::IsInvincible());
	}

	// 入力と時計はオブジェクトより先に確定させ、同じステップの全員が同じ値を見るようにします。
	const float realDeltaTime = GameTime::GetDeltaTime();
	SlowInput::Poll();
	SlowClock::Update(realDeltaTime, SlowInput::Get().Slow);

	World& world = GetWorld();
	const SlowPlayer* player = world.FindFirst<SlowPlayer>();
	if (player != nullptr && player->GetHp() <= 0)
	{
		// ゲームオーバー画面は作らず、倒れたら最初からやり直します。
		ChangeScene<SlowActionScene>();
		return;
	}

	// 全滅したら少し待って同じ配置で出し直します。検証を続けるための再配置で、ウェーブではありません。
	if (world.FindAll<SlowEnemy>().empty())
	{
		m_RespawnTimer += realDeltaTime;
		if (m_RespawnTimer >= GetSlowActionTuning().EnemyRespawnSeconds)
		{
			m_RespawnTimer = 0.0f;
			RespawnEnemies();
		}
	}
	else
	{
		m_RespawnTimer = 0.0f;
	}
	UpdateHud();
}

void SlowActionScene::UpdateHud()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	World& world = GetWorld();
	const SlowPlayer* player = world.FindFirst<SlowPlayer>();
	if (player == nullptr || m_Status == nullptr)
	{
		return;
	}

	constexpr int kGaugeCells = 20;
	const int filled = static_cast<int>(ceilf(SlowClock::GetGauge() / tuning.GaugeMax * kGaugeCells));
	std::wstring gauge;
	for (int i = 0; i < kGaugeCells; ++i)
	{
		gauge += i < filled ? L'■' : L'□';
	}

	std::wstring state;
	switch (player->GetState())
	{
	case SlowPlayer::State::Move: state = player->GetSpeed() > 0.1f ? L"移動" : L"待機"; break;
	case SlowPlayer::State::Dash: state = L"ダッシュ"; break;
	case SlowPlayer::State::Attack: state = std::wstring(AttackName(player->GetAttackType())) + L"攻撃"; break;
	}
	if (player->GetBufferedAttack() != SlowPlayer::AttackType::None)
	{
		state += std::wstring(L"  予約:") + AttackName(player->GetBufferedAttack());
	}
	if (player->IsDashLocked())
	{
		state += L"  [Shiftを離すまでダッシュ不可]";
	}
	if (player->GetAttackHoldSeconds() > 0.0f)
	{
		state += player->GetAttackHoldSeconds() >= tuning.StrongHoldSeconds ? L"  溜め:強" : L"  溜め:通常";
	}

	std::wstring text = Format(L"HP %d / %d\n", player->GetHp(), tuning.PlayerMaxHp);
	text += L"時間減速 " + gauge + Format(L" %.0f", SlowClock::GetGauge());
	if (SlowClock::GetPendingGain() > 0.0f)
	{
		text += Format(L"  (+%.0f 減速終了時に回復)", SlowClock::GetPendingGain());
	}
	text += Format(L"\n時間倍率 x%.2f%s", SlowClock::GetWorldScale(), SlowClock::IsSlowActive() ? L"  減速中" : L"");
	text += L"\nプレイヤー: " + state + Format(L"   追撃 %d回", player->GetPursuitCount());
	if (SlowEnemy::IsInvincible())
	{
		text += L"   [敵無敵]";
	}
	m_Status->SetText(text);
	m_Status->SetColor(SlowClock::IsSlowActive() ? XMFLOAT4{ 0.6f, 0.85f, 1.0f, 1.0f } : XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });

	std::wstring enemies;
	int index = 1;
	for (const SlowEnemy* enemy : world.FindAll<SlowEnemy>())
	{
		enemies += Format(L"敵%d  HP %d  %s", index++, enemy->GetHp(), EnemyStateName(enemy->GetState()));
		if (enemy->IsLaunched())
		{
			enemies += Format(L"  残り %.1f秒  壁反射 %d回%s", enemy->GetLaunchRemaining(),
				enemy->GetWallBounceCount(), enemy->IsHanging() ? L"  (滞空)" : L"");
		}
		enemies += L"\n";
	}
	if (enemies.empty())
	{
		enemies = L"敵なし(まもなく再配置)";
	}
	m_EnemyInfo->SetText(enemies);
}
