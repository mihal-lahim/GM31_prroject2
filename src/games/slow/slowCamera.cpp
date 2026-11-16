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
	if (Mouse_ConsumeDelta(&mouseX, &mouseY) && (mouseX != 0.0f || mouseY != 0.0f))
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
		const Vector3 target = player->GetPosition() + Vector3(0.0f, tuning.CameraFocusHeight, 0.0f);
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

	const float targetFov = dashing ? tuning.CameraDashFovDegrees : tuning.CameraFovDegrees;
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
	if (m_Shake > 0.0f)
	{
		m_ShakeTime += deltaTime;
		const Vector3 offset{ sinf(m_ShakeTime * 47.0f), sinf(m_ShakeTime * 59.0f + 1.3f), sinf(m_ShakeTime * 41.0f + 2.1f) };
		m_Eye += offset * m_Shake;
		focus += offset * m_Shake;
		m_Shake *= expf(-tuning.ShakeDecay * deltaTime);
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

	Engine::LIGHT light{};
	light.Enable = TRUE;
	light.Direction = { -0.45f, -0.78f, 0.43f, 0.0f };
	light.Diffuse = { blend(0.92f, 0.74f), blend(0.90f, 0.84f), blend(0.80f, 0.98f), 1.0f };
	light.Ambient = { blend(0.30f, 0.24f), blend(0.32f, 0.30f), blend(0.36f, 0.44f), 1.0f };
	light.SkyColor = { blend(0.50f, 0.40f), blend(0.66f, 0.60f), blend(0.82f, 0.92f), 1.0f };
	light.GroundColor = { 0.16f, 0.12f, 0.09f, 1.0f };
	light.GroundNormal = { 0.0f, 1.0f, 0.0f, 0.0f };
	Engine::Renderer::SetLight(light);
	Engine::Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FovDegrees),
		static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 200.0f));
	Engine::Renderer::SetViewMatrix(m_View);
	Engine::Renderer::SetCameraPosition(XMFLOAT3(m_Eye.x, m_Eye.y, m_Eye.z));
}

void SlowCamera::Shake(World& world, float strength)
{
	if (SlowCamera* camera = world.FindFirst<SlowCamera>())
	{
		camera->AddShake(strength);
	}
}
