#include "main.h"
#include "wireGiantMachine.h"

#include "gameTime.h"
#include "particleSystem.h"
#include "wireMachinePart.h"
#include "wireTpsCamera.h"
#include "world.h"

#include <algorithm>
#include <cmath>

namespace
{
	// 巨大機械の各部位の配置です。工場のプレス機が起き上がったような形を意図しています。
	// 「肩と腕は掛かり所、胸と背の装甲を割ると制御コアが出る」構成です。
	constexpr float kIdleSwaySpeed = 0.8f;
	constexpr float kIdleSwayAmount = 0.55f;
	constexpr float kRescueRouteHeight = 31.0f;
}

WireMachinePart* WireGiantMachine::AddPart(
	int role, const Vector3& localOffset, const Vector3& halfSize, int health)
{
	WireMachinePart* part = GetWorld().Spawn<WireMachinePart>();
	part->Setup(this, static_cast<WireMachinePart::Role>(role), localOffset, halfSize, health);
	part->SetPosition(m_Position + localOffset);
	m_Parts.push_back(part);
	return part;
}

void WireGiantMachine::Init()
{
	m_Layer = Layer::World;
	m_BasePosition = m_Position;

	using Role = WireMachinePart::Role;

	// 工場の天井(46m)に迫る高さにし、侵入地点から見上げた時点で
	// 「あれが目標だ」と一目で分かる大きさにしています。
	// --- 脚と胴。足場と掛かり所を兼ねる構造部です ---
	AddPart(static_cast<int>(Role::Structure), { -11.0f, 8.0f, 0.0f }, { 5.0f, 8.0f, 5.0f }, 1);
	AddPart(static_cast<int>(Role::Structure), { 11.0f, 8.0f, 0.0f }, { 5.0f, 8.0f, 5.0f }, 1);
	AddPart(static_cast<int>(Role::Structure), { 0.0f, 21.0f, 0.0f }, { 12.0f, 6.5f, 8.0f }, 1);

	// --- 肩。ワイヤーで飛び移るための高い掛かり所です ---
	AddPart(static_cast<int>(Role::Platform), { -18.0f, 30.0f, 0.0f }, { 6.0f, 2.2f, 6.0f }, 1);
	AddPart(static_cast<int>(Role::Platform), { 18.0f, 30.0f, 0.0f }, { 6.0f, 2.2f, 6.0f }, 1);

	// --- 腕。大きく張り出し、周囲を飛び回る足場になります ---
	AddPart(static_cast<int>(Role::Platform), { -25.0f, 22.0f, 9.0f }, { 3.6f, 2.0f, 11.0f }, 1);
	AddPart(static_cast<int>(Role::Platform), { 25.0f, 22.0f, 9.0f }, { 3.6f, 2.0f, 11.0f }, 1);

	// --- 頭部。てっぺんの目印になります ---
	AddPart(static_cast<int>(Role::Platform), { 0.0f, 34.0f, 0.0f }, { 5.0f, 3.0f, 5.0f }, 1);

	// --- 装甲。すべて割ると弱点が露出します ---
	AddPart(static_cast<int>(Role::Armor), { 0.0f, 24.0f, -8.6f }, { 9.5f, 5.5f, 2.0f }, 260);
	AddPart(static_cast<int>(Role::Armor), { 0.0f, 24.0f, 8.6f }, { 9.5f, 5.5f, 2.0f }, 260);
	AddPart(static_cast<int>(Role::Armor), { 0.0f, 30.0f, 0.0f }, { 8.0f, 3.4f, 7.0f }, 320);

	// --- 制御コア(弱点)。装甲が割れるまで攻撃できません ---
	AddPart(static_cast<int>(Role::WeakPoint), { 0.0f, 24.0f, 0.0f }, { 4.2f, 4.2f, 4.2f }, 420);
}

void WireGiantMachine::Uninit()
{
	m_Parts.clear();
	GameObject::Uninit();
}

void WireGiantMachine::Place(const Vector3& position)
{
	m_Position = position;
	m_BasePosition = position;
	// 部位は次のUpdateで追従しますが、初期表示のずれを避けて即座に合わせます。
	for (WireMachinePart* part : m_Parts)
	{
		part->SetPosition(m_Position + part->GetLocalOffset());
	}
}

void WireGiantMachine::Update()
{
	const float dt = GameTime::GetDeltaTime();
	m_DisableFlash = (std::max)(0.0f, m_DisableFlash - dt);

	if (!IsDisabled())
	{
		// ゆっくり上下に揺らして、機械が生きていることを見せます。
		// 揺れが大きいとワイヤーの狙いが難しくなるため、振幅は控えめです。
		m_IdleTime += dt;
		m_Position.y = m_BasePosition.y + sinf(m_IdleTime * kIdleSwaySpeed) * kIdleSwayAmount;
	}

	UpdatePhase();
	GameObject::Update();
}

void WireGiantMachine::UpdatePhase()
{
	if (m_Phase == Phase::Disabled)
	{
		return;
	}

	if (m_Phase == Phase::Armored && GetRemainingArmorCount() == 0)
	{
		// 装甲が全部割れたので、弱点を露出させます。
		m_Phase = Phase::Exposed;
		for (WireMachinePart* part : m_Parts)
		{
			if (part->GetRole() == WireMachinePart::Role::WeakPoint)
			{
				part->SetExposed(true);
			}
		}
		if (WireTpsCamera* camera = GetWorld().FindFirst<WireTpsCamera>())
		{
			camera->AddShake(0.55f);
		}
		return;
	}

	if (m_Phase != Phase::Exposed)
	{
		return;
	}

	for (WireMachinePart* part : m_Parts)
	{
		if (part->GetRole() == WireMachinePart::Role::WeakPoint && part->IsBroken())
		{
			m_Phase = Phase::Disabled;
			m_DisableFlash = 1.2f;
			if (WireTpsCamera* camera = GetWorld().FindFirst<WireTpsCamera>())
			{
				camera->AddShake(1.0f);
			}
			// 停止の合図として大きく火花を出します。
			for (int i = 0; i < 120; ++i)
			{
				const float angle = XM_2PI * static_cast<float>(i) / 120.0f;
				const float speed = 8.0f + static_cast<float>(i % 9) * 1.6f;
				ParticleSystem::Emit(
					m_Position + Vector3(0.0f, 18.0f, 0.0f),
					{ cosf(angle) * speed, 6.0f + static_cast<float>(i % 7), sinf(angle) * speed },
					0.30f, 60,
					(i % 4 == 0)
						? XMFLOAT4(1.0f, 1.0f, 0.85f, 1.0f)
						: XMFLOAT4(1.0f, 0.52f, 0.20f, 0.95f),
					-8.0f, 0.976f);
			}
			return;
		}
	}
}

void WireGiantMachine::OnPartBroken(WireMachinePart& part)
{
	(void)part;
	// 段階の判定はUpdatePhaseに集約しています。
	// 破壊の通知順に依存せず、常に現在の部位の状態から段階を決めるためです。
}

int WireGiantMachine::GetRemainingArmorCount() const
{
	int count = 0;
	for (const WireMachinePart* part : m_Parts)
	{
		if (part->GetRole() == WireMachinePart::Role::Armor && !part->IsBroken())
		{
			++count;
		}
	}
	return count;
}

int WireGiantMachine::GetTotalArmorCount() const
{
	int count = 0;
	for (const WireMachinePart* part : m_Parts)
	{
		if (part->GetRole() == WireMachinePart::Role::Armor)
		{
			++count;
		}
	}
	return count;
}

WireBlockField::Contact WireGiantMachine::ResolvePlayerCollision(
	const Vector3& previousPosition, float radius, Vector3& position, Vector3& velocity) const
{
	WireBlockField::Contact result;
	for (const WireMachinePart* part : m_Parts)
	{
		if (part->IsBroken())
		{
			// 壊れた装甲はすり抜けられます。破壊した先へ進めることが手応えになります。
			continue;
		}

		WireBlockField::Block block;
		block.Center = part->GetPosition();
		block.HalfSize = part->GetHalfSize();
		block.Landable = part->IsLandable();
		const WireBlockField::Contact contact = WireBlockField::ResolveSphereAgainstBlock(
			block, previousPosition, radius, position, velocity);
		if (contact.Touched)
		{
			result = contact;
		}
	}
	return result;
}

bool WireGiantMachine::FindGroundBelow(
	const Vector3& position, float footMargin, float maxDrop, float& groundY) const
{
	bool found = false;
	float best = position.y - maxDrop;

	for (const WireMachinePart* part : m_Parts)
	{
		if (part->IsBroken() || !part->IsLandable())
		{
			continue;
		}

		WireBlockField::Block block;
		block.Center = part->GetPosition();
		block.HalfSize = part->GetHalfSize();
		block.Landable = true;
		float candidate = 0.0f;
		if (!WireBlockField::BlockGroundBelow(block, position, footMargin, maxDrop, candidate)
			|| candidate < best)
		{
			continue;
		}

		best = candidate;
		found = true;
	}

	if (found)
	{
		groundY = best;
	}
	return found;
}

Vector3 WireGiantMachine::GetRescueRoutePosition() const
{
	// 背中側の高い位置です。弱点を壊すとここへ到達できるようになります。
	return m_Position + Vector3(0.0f, kRescueRouteHeight, -11.0f);
}
