#include "main.h"
#include "wireRobotRenderer.h"

#include "gameObject.h"
#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "wireBoxMesh.h"
#include "wireGameplayTuning.h"
#include "wirePlayer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	const XMFLOAT4 kCream{ 0.96f, 0.93f, 0.82f, 1.0f };
	const XMFLOAT4 kCyan{ 0.18f, 0.82f, 0.96f, 1.0f };
	const XMFLOAT4 kYellow{ 1.0f, 0.72f, 0.16f, 1.0f };
	const XMFLOAT4 kCoral{ 1.0f, 0.34f, 0.42f, 1.0f };
	const XMFLOAT4 kLime{ 0.55f, 0.90f, 0.30f, 1.0f };
	const XMFLOAT4 kInk{ 0.08f, 0.16f, 0.22f, 1.0f };

	const Vector3 kLeftShoulder{ -0.62f, 1.55f, 0.0f };
	const Vector3 kRightShoulder{ 0.62f, 1.55f, 0.0f };
	const Vector3 kLeftElbow{ -0.72f, 1.04f, 0.0f };
	const Vector3 kRightElbow{ 0.72f, 1.04f, 0.0f };
	const Vector3 kLeftHip{ -0.32f, 0.96f, 0.0f };
	const Vector3 kRightHip{ 0.32f, 0.96f, 0.0f };
	const Vector3 kLeftKnee{ -0.32f, 0.44f, 0.0f };
	const Vector3 kRightKnee{ 0.32f, 0.44f, 0.0f };
	const Vector3 kLeftAnkle{ -0.32f, 0.08f, 0.0f };
	const Vector3 kRightAnkle{ 0.32f, 0.08f, 0.0f };
	const Vector3 kNeck{ 0.0f, 1.60f, 0.0f };

	XMMATRIX Rotation(float pitch, float yaw, float roll)
	{
		return XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
	}
}

void WireRobotRenderer::Init()
{
	std::vector<Engine::VERTEX_3D> vertices;
	std::vector<unsigned int> indices;
	WireBoxMesh::Append(vertices, indices, { 0,0,0 }, { 1,1,1 }, { 1,1,1,1 }, true);

	D3D11_BUFFER_DESC vertexDesc{};
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Engine::VERTEX_3D));
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices.data();
	D3D11_BUFFER_DESC indexDesc{};
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(unsigned int));
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();

	m_Available = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, vertexDesc, &vertexData, "WireRobotVertices");
	m_Available = Engine::GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, indexDesc, &indexData, "WireRobotIndices") && m_Available;
	m_Available = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireRobot") && m_Available;
	m_IndexCount = static_cast<unsigned int>(indices.size());
}

void WireRobotRenderer::Uninit()
{
	m_VertexBuffer.Reset();
	m_IndexBuffer.Reset();
	m_VertexLayout.Reset();
	m_VertexShader.Reset();
	m_PixelShader.Reset();
	m_Available = false;
}

void WireRobotRenderer::Update()
{
	const float dt = Engine::GameTime::GetDeltaTime();
	m_AnimationTime += dt;
	m_LandTimer = (std::max)(0.0f, m_LandTimer - dt);
	m_JumpTimer = (std::max)(0.0f, m_JumpTimer - dt);
	m_WireShootTimer = (std::max)(0.0f, m_WireShootTimer - dt);
	m_StumbleTimer = (std::max)(0.0f, m_StumbleTimer - dt);
	if (m_Style != Style::Player || m_GameObject == nullptr) return;

	const WirePlayer* player = dynamic_cast<const WirePlayer*>(m_GameObject);
	if (player == nullptr) return;
	const WireAnimationTuning& tuning = GetWireGameplayTuning().Animation;
	const bool grounded = player->IsOnGround();
	const bool wire = player->IsWireActive();
	if (grounded && !m_PreviousGrounded)
	{
		m_LandTimer = tuning.LandDuration;
		m_LandImpact = std::clamp(-m_PreviousVelocity.y / 34.0f, 0.25f, 1.0f);
	}
	if (grounded) m_JumpPoseActive = false;
	if (!grounded && m_PreviousGrounded && player->GetVelocity().y > 0.0f)
	{
		m_JumpTimer = tuning.JumpPoseDuration;
		m_JumpPoseActive = true;
	}
	if (wire && !m_PreviousWireActive) m_WireShootTimer = tuning.ShootDuration;

	float targetRoll = 0.0f;
	if (wire)
	{
		const Vector3 rope = player->GetWirePivot() - player->GetPosition();
		const Vector3 velocity = player->GetVelocity();
		const float turn = rope.x * velocity.z - rope.z * velocity.x;
		if (std::abs(turn) > 0.05f)
		{
			targetRoll = (turn < 0.0f ? -1.0f : 1.0f) * tuning.WireSwingRoll
				* std::clamp(velocity.length() / 42.0f, 0.15f, 1.0f);
		}
	}
	m_WireRoll += (targetRoll - m_WireRoll) * std::clamp(dt * 12.0f, 0.0f, 1.0f);

	const Vector3 velocity = player->GetVelocity();
	if (!grounded && !wire && m_PreviousVelocity.length() - velocity.length() > 15.0f)
	{
		m_StumbleTimer = tuning.StumbleDuration;
	}
	m_PreviousGrounded = grounded;
	m_PreviousWireActive = wire;
	m_PreviousVelocity = velocity;
}

void WireRobotRenderer::Draw()
{
	if (!m_Available || m_GameObject == nullptr || m_IndexCount == 0) return;
	const WirePlayer* player = m_Style == Style::Player
		? dynamic_cast<const WirePlayer*>(m_GameObject) : nullptr;
	const WireAnimationTuning& animation = GetWireGameplayTuning().Animation;
	const WireGrappleTuning& grapple = GetWireGameplayTuning().Grapple;
	const Vector3 position = m_GameObject->GetPosition();
	const Vector3 ownerRotation = m_GameObject->GetRotation();
	const Vector3 ownerScale = m_GameObject->GetScale();
	const Vector3 velocity = player != nullptr ? player->GetVelocity() : Vector3{};
	const float horizontalSpeed = Vector3(velocity.x, 0.0f, velocity.z).length();
	const float speed = velocity.length();
	const bool grounded = player != nullptr && player->IsOnGround();
	const bool wire = player != nullptr && player->IsWireActive();
	const bool dash = player != nullptr && player->IsAirDashing();
	const bool wallContact = player != nullptr && !grounded && !wire && player->CanWallKick();

	const float move = std::clamp(horizontalSpeed / 16.0f, 0.0f, 1.0f);
	const float highSpeed = std::clamp((speed - animation.HighSpeedStart) / 24.0f, 0.0f, 1.0f);
	// 周期は移動速度を直接掛けず、この範囲で必ず飽和させます。
	const float frequency = animation.RunCycleSpeed * (0.55f + move * 0.45f);
	const float cycle = m_AnimationTime * frequency;
	const float gait = sinf(cycle)
		* (animation.WalkSwing + (animation.RunSwing - animation.WalkSwing) * move);
	const float idle = grounded ? 1.0f - move : 0.0f;
	const float land = m_LandTimer > 0.0f
		? std::clamp(m_LandTimer / animation.LandDuration, 0.0f, 1.0f) : 0.0f;
	const float landSquash = sinf(land * XM_PI) * animation.LandSquash * m_LandImpact;
	const float bob = sinf(m_AnimationTime * 3.0f) * 0.035f * idle
		+ (land > 0.0f ? 0.0f : std::abs(sinf(cycle)) * 0.045f * move);
	const float rising = !grounded && velocity.y > 1.0f
		? std::clamp(velocity.y / 14.0f, 0.0f, 1.0f) : 0.0f;
	const float falling = !grounded && velocity.y < -1.0f
		? std::clamp(-velocity.y / 30.0f, 0.0f, 1.0f) : 0.0f;
	const float jumpRemaining = m_JumpTimer > 0.0f
		? std::clamp(m_JumpTimer / animation.JumpPoseDuration, 0.0f, 1.0f) : 0.0f;
	const float jumpElapsed = 1.0f - jumpRemaining;
	const float jumpAnticipation = jumpRemaining > 0.0f
		? std::clamp(1.0f - jumpElapsed / 0.16f, 0.0f, 1.0f) : 0.0f;
	float jumpRaise = jumpRemaining > 0.0f
		? std::clamp((jumpElapsed - 0.04f) / 0.28f, 0.0f, 1.0f)
		: (m_JumpPoseActive ? 1.0f : 0.0f);
	jumpRaise = jumpRaise * jumpRaise * (3.0f - 2.0f * jumpRaise);
	const float apex = !grounded
		? std::clamp(1.0f - std::abs(velocity.y) / 5.0f, 0.0f, 1.0f) : 0.0f;
	const float release = player != nullptr ? player->GetWireReleasePose01() : 0.0f;
	const float releaseSign = player != nullptr ? player->GetWireReleaseRollSign() : 1.0f;
	const float releaseStrength = player != nullptr ? player->GetWireReleasePoseIntensity() : 0.0f;
	const float releaseRoll = releaseSign * grapple.ReleasePoseRoll * release * release * releaseStrength;
	const float releaseTwist = -releaseSign * grapple.ReleasePoseTwist * release * release * releaseStrength;
	const float airLean = dash ? animation.HighSpeedLean
		: animation.HighSpeedLean * highSpeed + animation.FallTilt * falling;
	const float stumble = m_StumbleTimer > 0.0f
		? sinf(m_StumbleTimer / animation.StumbleDuration * XM_PI) * 0.32f : 0.0f;

	const float bodyPitch = -move * 0.15f - rising * 0.10f - airLean * 0.35f + stumble;
	const float bodyRoll = m_WireRoll + releaseRoll;
	const float bodyYaw = releaseTwist;
	const XMMATRIX actor = XMMatrixScaling(ownerScale.x, ownerScale.y, ownerScale.z)
		* Rotation(ownerRotation.x, ownerRotation.y, ownerRotation.z)
		* XMMatrixTranslation(position.x, position.y, position.z);
	const float jumpSink = animation.JumpAnticipation * jumpAnticipation;
	const XMMATRIX bodyPose = Rotation(bodyPitch, bodyYaw, bodyRoll)
		* XMMatrixTranslation(0.0f, bob - landSquash * 0.32f - jumpSink, 0.0f);

	Engine::Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Engine::Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	UINT stride = sizeof(Engine::VERTEX_3D), offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	auto drawBox = [&](const Vector3& half, const XMFLOAT4& color, const XMMATRIX& local)
	{
		Engine::Renderer::SetWorldMatrix(XMMatrixScaling(half.x, half.y, half.z) * local * bodyPose * actor);
		Engine::MATERIAL material{};
		material.Diffuse = color;
		material.Ambient = color;
		material.TextureEnable = FALSE;
		Engine::Renderer::SetMaterial(material);
		Engine::Renderer::GetDeviceContext()->DrawIndexed(m_IndexCount, 0, 0);
	};
	auto jointPart = [&](const Vector3& center, const Vector3& pivot, const Vector3& half,
		const XMFLOAT4& color, float pitch, float yaw = 0.0f, float roll = 0.0f)
	{
		drawBox(half, color,
			XMMatrixTranslation(center.x - pivot.x, center.y - pivot.y, center.z - pivot.z)
			* Rotation(pitch, yaw, roll)
			* XMMatrixTranslation(pivot.x, pivot.y, pivot.z));
	};

	if (m_Style == Style::Drone)
	{
		const XMFLOAT4 droneBody{ 0.38f, 0.22f, 0.52f, 1.0f };
		const XMFLOAT4 droneWing{ 0.72f, 0.30f, 0.58f, 1.0f };
		const XMFLOAT4 droneEye{ 1.0f, 0.25f, 0.20f, 1.0f };
		const XMFLOAT4 droneAntenna{ 0.90f, 0.85f, 0.70f, 1.0f };
		const float hover = sinf(m_AnimationTime * 2.0f) * 0.15f;
		const float tilt = sinf(m_AnimationTime * 1.3f) * 0.08f;
		const float wingFlap = sinf(m_AnimationTime * 12.0f) * 0.35f;
		const XMMATRIX droneBody_ = Rotation(tilt, 0.0f, tilt * 0.5f)
			* XMMatrixTranslation(0.0f, hover, 0.0f);
		drawBox({ 0.55f, 0.40f, 0.45f }, droneBody, XMMatrixTranslation(0.0f, 0.9f, 0.0f) * droneBody_);
		drawBox({ 0.14f, 0.14f, 0.06f }, droneEye, XMMatrixTranslation(0.0f, 1.02f, 0.52f) * droneBody_);
		drawBox({ 0.06f, 0.22f, 0.06f }, droneAntenna, XMMatrixTranslation(-0.18f, 1.48f, 0.0f) * droneBody_);
		drawBox({ 0.06f, 0.18f, 0.06f }, droneAntenna, XMMatrixTranslation(0.18f, 1.42f, 0.0f) * droneBody_);
		auto drawWing = [&](float side)
		{
			const XMMATRIX wingPivot = XMMatrixTranslation(-side * 0.3f, 0.0f, 0.0f)
				* Rotation(0.0f, 0.0f, side * wingFlap)
				* XMMatrixTranslation(side * 0.85f, 1.0f, 0.0f) * droneBody_;
			drawBox({ 0.38f, 0.06f, 0.28f }, droneWing, wingPivot);
		};
		drawWing(-1.0f);
		drawWing(1.0f);
		return;
	}

	const XMFLOAT4 bodyColor = m_Style == Style::Player ? kCyan : kYellow;
	const XMFLOAT4 accent = m_Style == Style::Player ? kLime : kCoral;
	const float armLag = animation.LimbLag * highSpeed * 0.32f;
	float leftArm = grounded ? gait : -airLean - armLag;
	float rightArm = grounded ? -gait : -airLean - armLag;
	float leftArmRoll = 0.0f;
	float rightArmRoll = 0.0f;
	float leftLeg = grounded ? -gait : -rising * 0.35f - falling * 0.20f - highSpeed * 0.28f;
	float rightLeg = grounded ? gait : -rising * 0.35f - falling * 0.20f - highSpeed * 0.28f;
	if (!grounded && m_JumpPoseActive)
	{
		// 離陸直後だけ腕を軽く後ろへ引き、そのまま両肩から斜め上へ振り上げます。
		const float jumpArm = 0.30f * jumpAnticipation - animation.JumpArmRaise * jumpRaise;
		const float spread = (animation.JumpArmSpread + apex * 0.16f) * jumpRaise;
		leftArm = jumpArm;
		rightArm = jumpArm;
		leftArmRoll = -spread;
		rightArmRoll = spread;
		leftLeg -= 0.16f * jumpRaise;
		rightLeg -= 0.16f * jumpRaise;
	}
	if (!grounded && falling > 0.0f && !wire && !dash)
	{
		// 頂点から落下へ入ると、万歳姿勢を横へ開いてバランス姿勢へつなぎます。
		const float fallBlend = std::clamp(falling * 1.8f, 0.0f, 1.0f);
		leftArm = leftArm + (-1.20f - leftArm) * fallBlend;
		rightArm = rightArm + (-1.20f - rightArm) * fallBlend;
		leftArmRoll = leftArmRoll + (-0.72f - leftArmRoll) * fallBlend;
		rightArmRoll = rightArmRoll + (0.72f - rightArmRoll) * fallBlend;
	}
	if (land > 0.0f && !wire && !dash)
	{
		leftArm = 0.18f * land;
		rightArm = 0.18f * land;
		leftArmRoll *= 1.0f - land;
		rightArmRoll *= 1.0f - land;
	}
	if (wallContact) { leftArm = -0.48f; rightArm = 0.20f; leftLeg = 0.58f; rightLeg = -0.22f; }
	if (dash)
	{
		leftArm = 0.72f;
		rightArm = 0.72f;
		leftArmRoll = -0.12f;
		rightArmRoll = 0.12f;
	}
	if (wire || m_WireShootTimer > 0.0f)
	{
		Vector3 aim = player->GetWirePivot() - (position + Vector3{0.0f,1.45f,0.0f});
		if (aim.length() > 0.001f)
		{
			aim.normalize();
			const float c = cosf(ownerRotation.y), s = sinf(ownerRotation.y);
			const float localX = aim.x * c - aim.z * s;
			const float localZ = aim.x * s + aim.z * c;
			rightArm = -atan2f(localZ, -aim.y);
			rightArmRoll = atan2f(localX, -aim.y);
		}
	}
	const float elbowBend = grounded ? 0.12f + move * 0.18f : 0.24f + highSpeed * 0.24f;
	const float kneeBend = landSquash * 1.20f + rising * 0.32f + falling * 0.16f;

	// 胴体と装備はbodyPoseを共有し、傾いたときにパーツ間が離れないようにします。
	drawBox({ 0.58f * (1.0f + landSquash * 0.35f), 0.48f * (1.0f - landSquash), 0.42f },
		bodyColor, XMMatrixTranslation(0.0f, 1.22f, 0.0f));
	drawBox({ 0.48f,0.42f,0.18f }, accent, XMMatrixTranslation(0.0f,1.43f,-0.53f));
	drawBox({ 0.25f,0.12f,0.05f }, accent, XMMatrixTranslation(0.0f,1.26f,0.44f));

	// 頭と目は首を親にするため、胴体の傾きを継承しつつ小さな独立揺れだけを足します。
	const float headPitch = rising * 0.10f - stumble * 0.45f;
	const float headYaw = sinf(m_AnimationTime * 0.65f) * 0.055f * idle;
	const XMMATRIX headPose = XMMatrixTranslation(-kNeck.x,-kNeck.y,-kNeck.z)
		* Rotation(headPitch,headYaw,-bodyRoll*0.10f)
		* XMMatrixTranslation(kNeck.x,kNeck.y,kNeck.z);
	drawBox({0.72f,0.62f,0.62f},kCream,XMMatrixTranslation(0.0f,2.20f,0.0f)*headPose);
	const bool blink = idle > 0.8f && fmodf(m_AnimationTime,4.2f) > 4.05f;
	const float eyeHeight = 0.13f * std::clamp(1.0f-highSpeed*0.55f-(wire?0.18f:0.0f)+falling*0.30f,0.35f,1.35f)*(blink?0.18f:1.0f);
	drawBox({0.12f,eyeHeight,0.05f},kInk,XMMatrixTranslation(-0.28f,2.23f,0.64f)*headPose);
	drawBox({0.12f,eyeHeight,0.05f},kInk,XMMatrixTranslation( 0.28f,2.23f,0.64f)*headPose);

	// 腕は肩→肘、脚は股関節→膝→足首の順で親回転を継承します。
	auto drawArm = [&](bool left, float shoulderPitch, float shoulderRoll)
	{
		const Vector3 shoulder = left ? kLeftShoulder : kRightShoulder;
		const Vector3 elbow = left ? kLeftElbow : kRightElbow;
		const Vector3 upperCenter = left ? Vector3{-0.72f,1.32f,0.0f} : Vector3{0.72f,1.32f,0.0f};
		const Vector3 foreCenter = left ? Vector3{-0.79f,0.84f,0.0f} : Vector3{0.79f,0.84f,0.0f};
		const XMMATRIX upper = Rotation(shoulderPitch,0.0f,shoulderRoll);
		drawBox({0.17f,0.30f,0.20f},kCream,
			XMMatrixTranslation(upperCenter.x-shoulder.x,upperCenter.y-shoulder.y,0.0f)*upper*XMMatrixTranslation(shoulder.x,shoulder.y,0.0f));
		drawBox({0.16f,0.25f,0.18f},accent,
			XMMatrixTranslation(foreCenter.x-elbow.x,foreCenter.y-elbow.y,0.0f)
			* Rotation(left?-elbowBend:elbowBend,0.0f,0.0f)
			* XMMatrixTranslation(elbow.x-shoulder.x,elbow.y-shoulder.y,0.0f)
			* upper*XMMatrixTranslation(shoulder.x,shoulder.y,0.0f));
	};
	drawArm(true,leftArm,leftArmRoll);
	drawArm(false,rightArm,rightArmRoll);

	auto drawLeg = [&](bool left, float hipPitch)
	{
		const Vector3 hip=left?kLeftHip:kRightHip, knee=left?kLeftKnee:kRightKnee, ankle=left?kLeftAnkle:kRightAnkle;
		const Vector3 thighCenter=left?Vector3{-0.32f,0.70f,0.0f}:Vector3{0.32f,0.70f,0.0f};
		const Vector3 shinCenter=left?Vector3{-0.32f,0.22f,0.0f}:Vector3{0.32f,0.22f,0.0f};
		const XMMATRIX thigh=Rotation(hipPitch,0.0f,0.0f);
		drawBox({0.21f,0.27f,0.25f},accent,
			XMMatrixTranslation(thighCenter.x-hip.x,thighCenter.y-hip.y,0.0f)*thigh*XMMatrixTranslation(hip.x,hip.y,0.0f));
		const XMMATRIX shin=Rotation(kneeBend,0.0f,0.0f);
		drawBox({0.19f,0.23f,0.22f},kCream,
			XMMatrixTranslation(shinCenter.x-knee.x,shinCenter.y-knee.y,0.0f)*shin
			*XMMatrixTranslation(knee.x-hip.x,knee.y-hip.y,0.0f)*thigh*XMMatrixTranslation(hip.x,hip.y,0.0f));
		drawBox({0.24f,0.09f,0.34f},accent,
			XMMatrixTranslation(0.0f,-0.05f,0.12f)*XMMatrixTranslation(ankle.x-knee.x,ankle.y-knee.y,0.0f)
			*shin*XMMatrixTranslation(knee.x-hip.x,knee.y-hip.y,0.0f)*thigh*XMMatrixTranslation(hip.x,hip.y,0.0f));
	};
	drawLeg(true,leftLeg);
	drawLeg(false,rightLeg);
}
