#include "slowAction.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "renderer.h"
#include "gameSettings.h"
#include "gameTime.h"
#include "mouseInput.h"
#include "sceneEditor.h"
#include "world.h"

using namespace SlowUtil;

void SlowCamera::Init()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	m_Layer = Layer::Camera;
	m_Pitch = XMConvertToRadians(tuning.CameraDefaultPitchDegrees);
	m_Distance = tuning.CameraDistance;
	m_FovDegrees = tuning.CameraFovDegrees;
	Mouse_Reset();
}

Vector3 SlowCamera::GetCameraForward() const
{
	const float cosPitch = cosf(m_Pitch);
	return { sinf(m_Yaw) * cosPitch, -sinf(m_Pitch), cosf(m_Yaw) * cosPitch };
}

void SlowCamera::PreUpdate()
{
	if (SceneEditor::IsActive())
	{
		// 編集中の視点はエディタの自由カメラが決めます。
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	// 調整パネル(F4)などでカーソルが自由な間は、マウスはウィンドウの操作に使うので視点を回しません。
	// 移動量は毎フレーム読み捨て、閉じた瞬間に溜まった分で視点が飛ばないようにします(自動の向き補正は続けます)。
	const bool moved = Mouse_ConsumeDelta(&mouseX, &mouseY) && (mouseX != 0.0f || mouseY != 0.0f)
		&& !SceneEditor::IsCursorFree();
	if (moved)
	{
		const float sensitivity = GameSettings::GetMouseSensitivity();
		m_Yaw = WrapAngle(m_Yaw + mouseX * sensitivity);
		m_Pitch = std::clamp(m_Pitch + mouseY * sensitivity,
			XMConvertToRadians(tuning.CameraMinPitchDegrees), XMConvertToRadians(tuning.CameraMaxPitchDegrees));
		// マウス操作を優先し、自動の向き補正はそこで打ち切ります。
		m_AssistRemaining = 0.0f;
	}
	else if (m_AssistRemaining != 0.0f)
	{
		const float maxStep = XMConvertToRadians(tuning.CameraAssistDegreesPerSecond) * Engine::GameTime::GetDeltaTime();
		const float step = std::clamp(m_AssistRemaining, -maxStep, maxStep);
		m_Yaw = WrapAngle(m_Yaw + step);
		m_AssistRemaining -= step;
	}
}

void SlowCamera::AssistToward(const Vector3& worldPoint)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const Vector3 toPoint = Flatten(worldPoint - m_Focus);
	if (toPoint.length() < 0.5f)
	{
		return;
	}
	const float limit = XMConvertToRadians(tuning.CameraAssistMaxDegrees);
	const float difference = WrapAngle(YawFromDirection(toPoint) - m_Yaw);
	m_AssistRemaining = std::clamp(difference * tuning.CameraAssistFraction, -limit, limit);
}

void SlowCamera::SnapToTarget()
{
	m_HasFocus = false;
}

void SlowCamera::LateUpdate()
{
	if (SceneEditor::IsActive())
	{
		const SceneEditor::CameraPose& pose = SceneEditor::GetCamera();
		m_Yaw = pose.Yaw;
		m_Pitch = pose.Pitch;
		m_Eye = pose.Position;
		m_FovDegrees = pose.FovDegrees;
		const Vector3 lookAt = m_Eye + GetCameraForward();
		m_View = XMMatrixLookAtLH(
			XMVectorSet(m_Eye.x, m_Eye.y, m_Eye.z, 1.0f),
			XMVectorSet(lookAt.x, lookAt.y, lookAt.z, 1.0f),
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		// 再生へ戻ったとき、プレイヤーの後ろへ付け直します。
		m_HasFocus = false;
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float deltaTime = Engine::GameTime::GetDeltaTime();
	// 編集用カメラは真上や真下も向けるので、戻ってきたときにゲームの範囲へ収めます。
	m_Pitch = std::clamp(m_Pitch,
		XMConvertToRadians(tuning.CameraMinPitchDegrees), XMConvertToRadians(tuning.CameraMaxPitchDegrees));
	const SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
	const bool dashing = player != nullptr && player->GetState() == SlowPlayer::State::Dash;

	if (player != nullptr)
	{
		// 進む先へ注視点を少しずらします。止まると0へ戻るので、待機中の構図は変わりません。
		const Vector3 lookAheadTarget = LookAheadTarget(*player);
		if (!m_HasFocus)
		{
			m_LookAhead = lookAheadTarget;
		}
		else
		{
			m_LookAhead += (lookAheadTarget - m_LookAhead) * (std::min)(1.0f, (std::max)(tuning.CameraLookAheadRate, 0.0f) * deltaTime);
		}
		const Vector3 target = player->GetPosition() + Vector3(0.0f, tuning.CameraFocusHeight, 0.0f) + m_LookAhead;
		if (!m_HasFocus)
		{
			m_Focus = target;
			m_HasFocus = true;
		}
		else
		{
			// ダッシュ中だけ追従を緩めて、わずかに遅れてついていきます。
			const float rate = dashing ? tuning.CameraDashFollowRate : tuning.CameraFollowRate;
			m_Focus += (target - m_Focus) * (std::min)(1.0f, rate * deltaTime);
		}
	}

	// 減速中は深さに合わせて画角を少し狭め、時間が詰まった感じを出します。ダッシュの広がりにもそのまま重なります。
	const float targetFov = (dashing ? tuning.CameraDashFovDegrees : tuning.CameraFovDegrees)
		- tuning.CameraSlowFovNarrowDegrees * SlowTimeFx::GetIntensity();
	m_FovDegrees += (targetFov - m_FovDegrees) * (std::min)(1.0f, tuning.CameraFovBlendRate * deltaTime);

	// 注視点からカメラへレイを飛ばし、壁に遮られるならその手前まで寄せます。
	// 寄せるのは即座に、離すのはゆっくりにすると、柱の横を通るときに画面が跳ねません。
	const Vector3 backward = GetCameraForward() * -1.0f;
	float wantedDistance = tuning.CameraDistance;
	RaycastHit hit;
	if (GetWorld().GetCollisionWorld().Raycast(m_Focus, backward, tuning.CameraDistance + tuning.CameraWallMargin,
		hit, SlowCollisionLayers::World))
	{
		wantedDistance = std::clamp(hit.Distance - tuning.CameraWallMargin,
			(std::min)(tuning.CameraMinDistance, tuning.CameraDistance), tuning.CameraDistance);
	}
	if (wantedDistance < m_Distance)
	{
		m_Distance = wantedDistance;
	}
	else
	{
		m_Distance += (wantedDistance - m_Distance) * (std::min)(1.0f, 6.0f * deltaTime);
	}

	m_Eye = m_Focus + backward * m_Distance;
	Vector3 focus = m_Focus;
	// 揺れは注視点とカメラを同じだけずらします。向きが変わらないので酔いにくく、照準も狂いません。
	// 形(ShakeStyle)ごとに揺れ方と収まる速さを変えます。
	m_ShakeOffset = { 0.0f, 0.0f, 0.0f };
	if (m_Shake > 0.0f)
	{
		// 揺れはヒットストップで止めず(止めの間こそ画面を揺らす)、ポーズと時間の倍率だけを受けます。
		const float shakeDeltaTime = Engine::GameTime::GetChannelDeltaTime(Engine::GameTime::kHitStopFreeChannel);
		m_ShakeTime += shakeDeltaTime;
		m_ShakeOffset = ShakeShape() * m_Shake;
		m_Eye += m_ShakeOffset;
		focus += m_ShakeOffset;
		const float decayScale = m_ShakeStyle == ShakeStyle::Vertical ? 1.4f
			: m_ShakeStyle == ShakeStyle::Punch ? 1.15f : 0.75f;
		m_Shake *= expf(-tuning.ShakeDecay * decayScale * shakeDeltaTime);
		if (m_Shake < 0.002f)
		{
			m_Shake = 0.0f;
		}
	}
	m_View = XMMatrixLookAtLH(
		XMVectorSet(m_Eye.x, m_Eye.y, m_Eye.z, 1.0f),
		XMVectorSet(focus.x, focus.y, focus.z, 1.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

void SlowCamera::Draw()
{
	// 時間減速の深さ(0=通常、1=完全に減速)で照明を青く寄せ、減速中だと一目で分かるようにします。
	const float slowTimeScale = GetSlowActionTuning().SlowTimeScale;
	const float slowAmount = slowTimeScale < 1.0f
		? std::clamp((1.0f - SlowClock::GetWorldScale()) / (1.0f - slowTimeScale), 0.0f, 1.0f)
		: 0.0f;
	const auto blend = [slowAmount](float normal, float slow) { return normal + (slow - normal) * slowAmount; };

	// 太陽の向き・明るさ・色温度と、空・地面の照り返しは調整値から毎フレーム決めます(調整パネルで動かすとすぐ反映)。
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float azimuth = XMConvertToRadians(tuning.SunAzimuthDegrees);
	const float elevation = XMConvertToRadians(std::clamp(tuning.SunElevationDegrees, 10.0f, 85.0f));
	// 太陽へ向かう向きの逆が、光の進む向きです。
	const Vector3 toSun{ cosf(elevation) * sinf(azimuth), sinf(elevation), cosf(elevation) * cosf(azimuth) };
	// 色温度: 0=青白い昼、0.5=ふだんの色(以前の固定値)、1=夕日のような暖色。
	const float warmth = std::clamp(tuning.SunWarmth, 0.0f, 1.0f);
	const auto warm = [warmth](float cool, float normal, float sunset)
	{
		return warmth < 0.5f ? cool + (normal - cool) * (warmth * 2.0f) : normal + (sunset - normal) * ((warmth - 0.5f) * 2.0f);
	};
	const float sunIntensity = (std::max)(tuning.SunIntensity, 0.0f);
	const float skyIntensity = (std::max)(tuning.SkyIntensity, 0.0f);
	const float groundIntensity = (std::max)(tuning.GroundBounceIntensity, 0.0f);

	Engine::LIGHT light{};
	light.Enable = TRUE;
	light.Direction = { -toSun.x, -toSun.y, -toSun.z, 0.0f };
	light.Diffuse = { blend(warm(0.88f, 0.92f, 1.00f), 0.74f) * sunIntensity,
		blend(warm(0.92f, 0.90f, 0.72f), 0.84f) * sunIntensity,
		blend(warm(0.98f, 0.80f, 0.46f), 0.98f) * sunIntensity, 1.0f };
	light.Ambient = { blend(0.30f, 0.24f), blend(0.32f, 0.30f), blend(0.36f, 0.44f), 1.0f };
	light.SkyColor = { blend(0.50f, 0.40f) * skyIntensity, blend(0.66f, 0.60f) * skyIntensity,
		blend(0.82f, 0.92f) * skyIntensity, 1.0f };
	light.GroundColor = { 0.16f * groundIntensity, 0.12f * groundIntensity, 0.09f * groundIntensity, 1.0f };
	light.GroundNormal = { 0.0f, 1.0f, 0.0f, 0.0f };
	Engine::Renderer::SetLight(light);
	Engine::Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FovDegrees),
		static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 200.0f));
	Engine::Renderer::SetViewMatrix(m_View);
	Engine::Renderer::SetCameraPosition(XMFLOAT3(m_Eye.x, m_Eye.y, m_Eye.z));
}

void SlowCamera::AddShake(float strength, ShakeStyle style)
{
	if (strength <= 0.0f || strength < m_Shake)
	{
		return;
	}
	// 強い揺れが来たら、形も始まりからやり直します(弱い揺れの続きに強い形が混ざらないように)。
	m_Shake = strength;
	m_ShakeStyle = style;
	m_ShakeTime = 0.0f;
	m_ShakePhase = RandomRange(0.0f, XM_2PI);
	m_PunchSide = RandomRange(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
}

Vector3 SlowCamera::ShakeShape() const
{
	const float t = m_ShakeTime;
	// 画面の横(カメラの右)と縦で揺らすと、どの向きから見ても同じ手触りになります。
	const Vector3 right = RightFromYaw(m_Yaw);
	const Vector3 up{ 0.0f, 1.0f, 0.0f };
	switch (m_ShakeStyle)
	{
	case ShakeStyle::Vertical:
		// 縦の突き上げが主。横はわずかに添えるだけです。
		return up * sinf(t * 58.0f) + right * (0.18f * sinf(t * 37.0f + 0.7f));
	case ShakeStyle::Punch:
	{
		// 最初の約0.06秒に、縦横へ鋭く一度振ってから細かく揺れ戻します。
		const float kick = expf(-t * 30.0f);
		const float swing = cosf(t * 72.0f);
		return right * (m_PunchSide * (0.9f * kick + 0.55f * swing))
			+ up * (0.75f * kick + 0.5f * sinf(t * 66.0f + 0.4f));
	}
	case ShakeStyle::Heavy:
	default:
		// ゆっくりした大きな多軸の揺れ。始まりの位相を毎回変えて、同じ揺れに見えないようにします。
		return { sinf(t * 39.0f + m_ShakePhase), sinf(t * 47.0f + m_ShakePhase * 1.7f + 1.3f),
			sinf(t * 31.0f + m_ShakePhase * 0.6f + 2.1f) };
	}
}

Vector3 SlowCamera::LookAheadTarget(const SlowPlayer& player) const
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float distance = (std::max)(tuning.CameraLookAheadDistance, 0.0f);
	if (distance <= 0.0f)
	{
		return { 0.0f, 0.0f, 0.0f };
	}
	switch (player.GetState())
	{
	case SlowPlayer::State::Attack:
	case SlowPlayer::State::Charge:
	{
		// 攻撃と溜めは、振る向きの先を見せます。
		Vector3 direction = Flatten(player.GetAttackDirection());
		if (direction.length() < 0.01f) return { 0.0f, 0.0f, 0.0f };
		direction.normalize();
		return direction * distance;
	}
	default:
	{
		// 歩き・ダッシュは進む向きへ、速さに比例してずらします(歩きの最高速で全量。止まれば0)。
		Vector3 velocity = Flatten(player.GetVelocity());
		const float speed = velocity.length();
		if (speed < 0.05f) return { 0.0f, 0.0f, 0.0f };
		const float amount = (std::min)(1.0f, speed / (std::max)(tuning.WalkSpeed, 0.1f));
		return velocity / speed * (distance * amount);
	}
	}
}

void SlowCamera::Shake(World& world, float strength, ShakeStyle style)
{
	if (SlowCamera* camera = world.FindFirst<SlowCamera>())
	{
		camera->AddShake(strength, style);
	}
}

void SlowHitShake::Start(float amplitude, bool vertical)
{
	if (amplitude <= 0.0f)
	{
		return;
	}
	m_Amplitude = (std::max)(m_Amplitude, amplitude);
	m_Vertical = m_Vertical || vertical;
}

void SlowHitShake::Clear(GameObject& owner)
{
	m_Amplitude = 0.0f;
	m_Vertical = false;
	m_Phase = 0.0f;
	owner.SetDrawOffset({ 0.0f, 0.0f, 0.0f });
}

void SlowHitShake::Apply(GameObject& owner, bool airborne)
{
	const float remaining = Engine::GameTime::GetHitStopRemainingRatio();
	if (m_Amplitude <= 0.0f || remaining <= 0.0f)
	{
		// ヒットストップが終わったら、描く位置を必ず元へ戻します。
		Clear(owner);
		return;
	}
	// 1フレームごとに左右へ振ります(ポーズ中は時間が止まるので、震えもその形で止まります)。
	m_Phase += Engine::GameTime::GetChannelDeltaTime(Engine::GameTime::kHitStopFreeChannel) * 60.0f;
	float yaw = 0.0f;
	float distanceScale = 1.0f;
	if (const SlowCamera* camera = owner.GetWorld().FindFirst<SlowCamera>())
	{
		yaw = camera->GetYaw();
		const float distance = (owner.GetPosition() - camera->GetCameraPosition()).length();
		distanceScale = std::clamp(distance / kReferenceDistance, 0.85f, 1.8f);
	}
	const float amplitude = m_Amplitude * remaining * distanceScale;
	Vector3 offset = RightFromYaw(yaw) * (cosf(m_Phase * XM_PI) * amplitude);
	if (m_Vertical || airborne)
	{
		offset.y = sinf(m_Phase * XM_PI * 0.5f + 0.6f) * amplitude * 0.8f;
	}
	owner.SetDrawOffset(offset);
}
