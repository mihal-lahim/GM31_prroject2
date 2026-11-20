#include "slowAction.h"
#include "inputActions.h"

#include "slowActionTuning.h"
#include "slowTuningPanel.h"
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
#include "slowSwordTrail.h"

#include <cstdarg>

namespace
{
	constexpr const char* kStagePath = "assets/scene/slow-stage.scene";
	constexpr const wchar_t* kPlayHelp =
		L"WASD:移動  マウス:視点  Shift:ダッシュ  左クリック:短押し=通常 / 長押し=溜め(動けない)→離して強攻撃  "
		L"右クリック:時間減速  R:やり直し  F1:当たり判定  F2:敵無敵  F3:編集モード  F4:調整  Esc:終了";
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
	// 設定ファイルに調整値の一覧に無いキーがあったときの警告文です。空なら問題なしです。
	std::wstring g_TuningWarning;

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

	const wchar_t* EnemyKindName(SlowEnemy::Kind kind)
	{
		switch (kind)
		{
		case SlowEnemy::Kind::Gunner: return L"射撃";
		case SlowEnemy::Kind::Brute: return L"重量";
		default: return L"近接";
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
	// 調整パネルの上にマウスがあるとき・文字入力中は、攻撃や視点の操作を止めます。
	if (SlowTuningPanel::BlocksGameInput())
	{
		g_Input = SlowInputState{};
		return;
	}
	SlowInputState state;
	state.MoveZ = Engine::InputActions::GetAxis("move_back", "move_forward");
	state.MoveX = Engine::InputActions::GetAxis("move_left", "move_right");
	state.Dash = Engine::InputActions::IsDown("dash");
	state.Attack = Engine::InputActions::IsDown("attack");
	state.Slow = Engine::InputActions::IsDown("slow");
	g_Input = state;
}

void SlowInput::DefineDefaults()
{
	using Engine::InputActions;
	using Engine::InputBinding;
	InputActions::Define("move_forward", { InputBinding::Key(KK_W), InputBinding::PadAxis(Engine::PadLeftY, 1) });
	InputActions::Define("move_back", { InputBinding::Key(KK_S), InputBinding::PadAxis(Engine::PadLeftY, -1) });
	InputActions::Define("move_right", { InputBinding::Key(KK_D), InputBinding::PadAxis(Engine::PadLeftX, 1) });
	InputActions::Define("move_left", { InputBinding::Key(KK_A), InputBinding::PadAxis(Engine::PadLeftX, -1) });
	InputActions::Define("dash", { InputBinding::Key(KK_LEFTSHIFT), InputBinding::PadButton(Engine::PadButtons::A) });
	InputActions::Define("attack", { InputBinding::MouseLeft(), InputBinding::PadButton(Engine::PadButtons::X) });
	InputActions::Define("slow", { InputBinding::MouseRight(), InputBinding::PadAxis(Engine::PadLeftTrigger, 1) });
	InputActions::Define("retry", { InputBinding::Key(KK_R), InputBinding::PadButton(Engine::PadButtons::Back) });
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
	Engine::GameTime::SetChannelScale(SlowClock::kWorldTimeChannel, 1.0f);
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
	// 敵側の時間はEngineの時間チャンネルに載せます。ヒットストップやポーズも同じく効きます。
	Engine::GameTime::SetChannelScale(SlowClock::kWorldTimeChannel, g_WorldScale);
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

void SlowClock::AddGaugeNow(float amount)
{
	g_Gauge = (std::min)(GetSlowActionTuning().GaugeMax, g_Gauge + amount);
}

void SlowBlock::Init()
{
	m_Layer = Layer::World;
	// モデルは-1〜1の立方体なので、scaleがそのまま見た目と当たり判定の半サイズになります。
	// 壁用と障害物用の色違いを両方持ち、Obstacleの値で見せる方を切り替えます。
	// 壁は風化したコンクリートの大きなブロック、障害物は暗い鉄混じりの石柱で、粗さと金属度で質感を分けます。
	const auto addModel = [this](const char* path, float roughness, float metallic, float tileScale, float weathering)
	{
		ModelRenderer* model = AddComponent<ModelRenderer>()->SetUseOwnerTransform();
		model->Load(path);
		if (model->SetShaderDefinition("realistic-arena"))
		{
			model->SetShaderFloat("roughness", roughness);
			model->SetShaderFloat("metallic", metallic);
			model->SetShaderFloat("tileScale", tileScale);
			model->SetShaderFloat("weathering", weathering);
		}
		return model;
	};
	m_WallModel = addModel("assets\\model\\slowWall.obj", 0.88f, 0.0f, 1.6f, 0.75f);
	m_ObstacleModel = addModel("assets\\model\\slowObstacle.obj", 0.52f, 0.45f, 0.8f, 0.55f);
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
	// 床は暗めの石畳(2mの敷石)です。
	if (model->SetShaderDefinition("realistic-arena"))
	{
		model->SetShaderFloat("roughness", 0.82f);
		model->SetShaderFloat("metallic", 0.0f);
		model->SetShaderFloat("tileScale", 2.0f);
		model->SetShaderFloat("weathering", 0.6f);
	}
}

void SlowActionScene::Init()
{
	Collider::SetDebugDrawEnabled(false);
	SlowEnemy::SetInvincible(false);
	SlowClock::Reset();
	SlowTimeFx::BeginScene();
	// 設定ファイルのキー名の書き間違いは黙って既定値になるので、画面に出して気付けるようにします。
	g_TuningWarning.clear();
	for (const std::string& key : FindUnknownSlowActionTuningKeys())
	{
		g_TuningWarning += (g_TuningWarning.empty() ? L"[設定ファイルに不明なキー] " : L", ") + std::wstring(key.begin(), key.end());
	}
	SlowScore::Reset();
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
	world.Spawn<SlowSwordTrail>();
	world.Spawn<SlowSoundBank>();
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
	// スコアとコンボは右上に、目立つ出来事はその下へ流します。
	m_ScoreText = world.Spawn<DynamicText>();
	m_ScoreText->SetFontSize(22);
	m_ScoreText->SetPosition({ SCREEN_WIDTH - 330.0f, 12.0f, 0.0f });
	m_ComboText = world.Spawn<DynamicText>();
	m_ComboText->SetFontSize(40);
	m_ComboText->SetPosition({ SCREEN_WIDTH - 330.0f, 70.0f, 0.0f });
	m_PopupText = world.Spawn<DynamicText>();
	m_PopupText->SetFontSize(20);
	m_PopupText->SetPosition({ SCREEN_WIDTH - 330.0f, 170.0f, 0.0f });
	m_RespawnTimer = 0.0f;
	m_WasSlow = false;
	m_ClearAwarded = false;
	UpdateHud();
}

void SlowActionScene::Uninit()
{
	// やり直し・倒れたとき・アプリの終了のどれでも、減速の画面演出を残さないようにします。
	SlowTimeFx::Reset();
	// 調整パネルを閉じ、マウスの捕捉と時間の制御(停止・速さ)を通常へ戻します。
	SlowTuningPanel::Reset();
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
	// 弾と亡骸はステージの記述に含まれないので、別に片付けます。
	for (SlowBullet* bullet : world.FindAll<SlowBullet>())
	{
		bullet->MarkForDestroy();
	}
	for (SlowCorpse* corpse : world.FindAll<SlowCorpse>())
	{
		corpse->MarkForDestroy();
	}
	Engine::SceneSerializer::Read(world, g_StageText);
	SlowClock::Reset();
	// 編集中は演出が更新されないので、ここで消します。光の粒は残り、寿命で消えます。
	SlowTimeFx::Reset();
	SlowScore::Reset();
	m_RespawnTimer = 0.0f;
	m_ClearAwarded = false;

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
	// 調整パネルの上にマウスがあるとき・パネルで文字を入力しているときも、ゲームのキー操作を止めます。
	const bool panelBlocks = SlowTuningPanel::BlocksGameInput();
	if (!SceneEditor::WantsKeyboard())
	{
		if (Keyboard_IsKeyDownTrigger(KK_F4))
		{
			SlowTuningPanel::Toggle();
		}
		if (Keyboard_IsKeyDownTrigger(KK_F3))
		{
			ToggleEditMode();
		}
		if (!panelBlocks && Keyboard_IsKeyDownTrigger(KK_F1))
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
		m_ScoreText->SetText(L"");
		m_ComboText->SetText(L"");
		m_PopupText->SetText(L"");
		m_Help->SetText(L"編集モード  F3:再生に戻る  右ドラッグ:見回す  WASD/QE:移動(Shiftで加速)  ホイール:前後  "
			L"左クリック:選択  F1:当たり判定表示  F4:調整");
		return;
	}
	m_Help->SetText(kPlayHelp);

	// やり直しは、R キーかパネルの「やり直す」ボタンで行います。
	const bool retryRequested = SlowTuningPanel::TakeRetryRequest()
		|| (!panelBlocks && Engine::InputActions::IsTriggered("retry"));
	if (retryRequested)
	{
		// 遷移の間も前のシーンが描かれるので、減速の演出とパネルの状態はここで戻しておきます。
		SlowTimeFx::Reset();
		SlowTuningPanel::Reset();
		ChangeSceneWith<SlowActionScene>(SceneTransition::Instant());
		return;
	}
	if (!panelBlocks && Keyboard_IsKeyDownTrigger(KK_F2))
	{
		SlowEnemy::SetInvincible(!SlowEnemy::IsInvincible());
	}
	// 調整パネルのテスト用の近道です(HP全回復・時間ゲージ満タン)。
	if (SlowTuningPanel::TakeRefillHpRequest())
	{
		if (SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>())
		{
			player->SetHp(GetSlowActionTuning().PlayerMaxHp);
		}
	}
	if (SlowTuningPanel::TakeFillGaugeRequest())
	{
		SlowClock::AddGaugeNow(GetSlowActionTuning().GaugeMax);
	}

	// 入力と時計はオブジェクトより先に確定させ、同じステップの全員が同じ値を見るようにします。
	const float realDeltaTime = Engine::GameTime::GetDeltaTime();
	SlowInput::Poll();
	SlowClock::Update(realDeltaTime, SlowInput::Get().Slow);
	SlowScore::Update(realDeltaTime, SlowClock::GetWorldDeltaTime());
	if (SlowClock::IsSlowActive() && !m_WasSlow)
	{
		SlowSound::Play(SlowSound::Id::TimeOn);
		CheckJustSlow();
	}
	m_WasSlow = SlowClock::IsSlowActive();

	World& world = GetWorld();
	const SlowPlayer* player = world.FindFirst<SlowPlayer>();
	SlowTimeFx::Update(realDeltaTime, player != nullptr ? player->GetPosition() : Vector3(0.0f, 0.0f, 0.0f));
	if (player != nullptr && player->GetHp() <= 0)
	{
		// ゲームオーバー画面は作らず、倒れたら最初からやり直します。フェード中に演出を残さないよう、先に消します。
		SlowTimeFx::Reset();
		ChangeScene<SlowActionScene>();
		return;
	}

	// 全滅したら少し待って同じ配置で出し直します。検証を続けるための再配置で、ウェーブではありません。
	if (world.FindAll<SlowEnemy>().empty())
	{
		if (!m_ClearAwarded)
		{
			m_ClearAwarded = true;
			SlowScore::Add(SlowScore::Event::Clear);
		}
		m_RespawnTimer += realDeltaTime;
		if (m_RespawnTimer >= GetSlowActionTuning().EnemyRespawnSeconds)
		{
			m_RespawnTimer = 0.0f;
			m_ClearAwarded = false;
			RespawnEnemies();
		}
	}
	else
	{
		m_RespawnTimer = 0.0f;
	}
	UpdateHud();
}

void SlowActionScene::CheckJustSlow()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	World& world = GetWorld();
	const SlowPlayer* player = world.FindFirst<SlowPlayer>();
	if (player == nullptr)
	{
		return;
	}
	// 同時に複数の攻撃が迫っていれば、その数だけ報います。危ないほど見返りが大きくなります。
	int count = 0;
	for (SlowEnemy* enemy : world.FindAll<SlowEnemy>())
	{
		if (enemy->TryJustSlow(tuning.JustSlowWindowSeconds))
		{
			count++;
		}
	}
	for (SlowBullet* bullet : world.FindAll<SlowBullet>())
	{
		if (bullet->TryJustSlow(player->GetPosition(), tuning.JustSlowBulletRange))
		{
			count++;
		}
	}
	for (int i = 0; i < count; ++i)
	{
		SlowClock::AddGaugeNow(tuning.JustSlowGaugeGain);
		SlowScore::Add(SlowScore::Event::JustSlow);
	}
	if (count > 0)
	{
		// 発動したプレイヤー側の手応えなので、火花は実時間(チャンネル0)で弾けさせます。
		SlowUtil::EmitBurst(player->GetPosition() + Vector3(0.0f, 1.0f, 0.0f), 20, 7.0f, 0.3f, 20, { 0.5f, 0.85f, 1.0f, 1.0f });
		SlowTimeFx::TriggerJustSlowFlash();
		SlowSound::Play(SlowSound::Id::Deflect);
	}
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
	case SlowPlayer::State::Charge: state = L"溜め(動けない)"; break;
	case SlowPlayer::State::Attack:
		state = std::wstring(AttackName(player->GetAttackType())) + L"攻撃";
		if (player->GetComboStep() > 0)
		{
			state += Format(L"(%d段目)", player->GetComboStep());
		}
		break;
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
	if (!g_TuningWarning.empty())
	{
		text += L"\n" + g_TuningWarning;
	}
	m_Status->SetText(text);
	m_Status->SetColor(SlowClock::IsSlowActive() ? XMFLOAT4{ 0.6f, 0.85f, 1.0f, 1.0f } : XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });

	std::wstring enemies;
	int index = 1;
	for (const SlowEnemy* enemy : world.FindAll<SlowEnemy>())
	{
		enemies += Format(L"敵%d %s  HP %d  %s", index++, EnemyKindName(enemy->GetKind()), enemy->GetHp(),
			EnemyStateName(enemy->GetState()));
		if (enemy->GetKind() == SlowEnemy::Kind::Brute)
		{
			enemies += Format(L"  装甲 %d", enemy->GetArmor());
		}
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

	m_ScoreText->SetText(Format(L"SCORE %d\nBEST %d   最大コンボ %d",
		SlowScore::GetScore(), SlowScore::GetBestScore(), SlowScore::GetBestCombo()));
	const int combo = SlowScore::GetCombo();
	std::wstring popups;
	if (combo > 0)
	{
		m_ComboText->SetText(Format(L"%d COMBO %s\nx%.1f", combo, SlowScore::GetRank(), SlowScore::GetMultiplier()));
		// 評価が上がるほど色を熱くします。
		const float heat = (std::min)(static_cast<float>(combo) / 25.0f, 1.0f);
		m_ComboText->SetColor({ 1.0f, 1.0f - heat * 0.55f, 1.0f - heat * 0.9f, 1.0f });
		// 残り時間を小さな文字のバーで見せ、切れそうなことが分かるようにします。
		constexpr int kBarCells = 14;
		const int remaining = static_cast<int>(ceilf(SlowScore::GetComboRemaining() * kBarCells));
		for (int i = 0; i < kBarCells; ++i)
		{
			popups += i < remaining ? L'■' : L'□';
		}
		popups += L"\n";
	}
	else
	{
		m_ComboText->SetText(L"");
	}
	for (const SlowScore::Popup& popup : SlowScore::GetPopups())
	{
		popups += popup.Text + L"\n";
	}
	m_PopupText->SetText(popups);
	m_PopupText->SetColor({ 1.0f, 0.95f, 0.6f, 1.0f });
}
