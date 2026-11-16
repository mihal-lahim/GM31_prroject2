#include "cloth.h"

#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "renderer.h"
#include "shadowMap.h"
#include "world.h"

#include <algorithm>
#include <cmath>

namespace Engine
{
namespace
{
	Vector3 TransformPoint(const XMFLOAT4X4& matrix, const Vector3& point)
	{
		const XMVECTOR transformed = XMVector3TransformCoord(
			XMVectorSet(point.x, point.y, point.z, 1.0f), XMLoadFloat4x4(&matrix));
		return { XMVectorGetX(transformed), XMVectorGetY(transformed), XMVectorGetZ(transformed) };
	}

	Vector3 Lerp(const Vector3& from, const Vector3& to, float t)
	{
		return from + (to - from) * t;
	}

	Vector3 ClosestPointOnSegment(const Vector3& point, const Vector3& start, const Vector3& end)
	{
		const Vector3 segment = end - start;
		const float lengthSquared = Vector3::dot(segment, segment);
		if (lengthSquared <= 1e-8f)
		{
			return start;
		}
		const float t = std::clamp(Vector3::dot(point - start, segment) / lengthSquared, 0.0f, 1.0f);
		return start + segment * t;
	}

	bool IsFiniteVector(const Vector3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}
}

//----------------------------------------------------------------------------
// ClothSimulation
//----------------------------------------------------------------------------

void ClothSimulation::Build(const ClothSettings& settings)
{
	m_Settings = settings;
	m_Settings.PanelCount = (std::max)(1, m_Settings.PanelCount);
	m_Settings.ColumnsPerPanel = (std::max)(2, m_Settings.ColumnsPerPanel);
	m_Settings.Rows = (std::max)(2, m_Settings.Rows);
	m_Settings.Mass = (std::max)(1e-5f, m_Settings.Mass);

	const int panels = m_Settings.PanelCount;
	const int columns = m_Settings.ColumnsPerPanel;
	const int rows = m_Settings.Rows;
	const float width = (std::max)(0.01f, m_Settings.Width);
	const float height = (std::max)(0.01f, m_Settings.Height);
	const float gap = panels > 1 ? (std::max)(0.0f, m_Settings.PanelGap) : 0.0f;
	const float panelWidth = (std::max)(0.01f, (width - gap * (panels - 1)) / panels);
	const float spacingX = panelWidth / (columns - 1);
	const float spacingY = height / (rows - 1);
	const float particleMass = m_Settings.Mass / static_cast<float>(panels * columns * rows);

	m_Particles.clear();
	m_Springs.clear();
	m_Triangles.clear();
	m_Particles.reserve(static_cast<std::size_t>(panels * columns * rows));

	auto index = [columns, rows](int panel, int column, int row)
	{
		return panel * columns * rows + row * columns + column;
	};

	for (int panel = 0; panel < panels; ++panel)
	{
		const float left = -width * 0.5f + panel * (panelWidth + gap);
		for (int row = 0; row < rows; ++row)
		{
			for (int column = 0; column < columns; ++column)
			{
				Particle particle;
				particle.LocalRest = { left + column * spacingX, -row * spacingY, 0.0f };
				particle.Position = particle.LocalRest;
				// 上端の行をのれん棒に固定します。授業教材のonLockに当たります。
				particle.Pinned = row == 0;
				particle.InverseMass = particle.Pinned ? 0.0f : 1.0f / particleMass;
				particle.U = (particle.LocalRest.x + width * 0.5f) / width;
				particle.V = static_cast<float>(row) / (rows - 1);
				m_Particles.push_back(particle);
			}
		}
	}

	auto addSpring = [&](int a, int b, float stiffness, bool structural)
	{
		if (m_Particles[a].Pinned && m_Particles[b].Pinned)
		{
			return;
		}
		Spring spring;
		spring.A = a;
		spring.B = b;
		spring.RestLength = (m_Particles[b].LocalRest - m_Particles[a].LocalRest).length();
		spring.Stiffness = stiffness;
		// 臨界減衰 2*sqrt(k*m) に対する比率で与えるので、質量や硬さを変えても減衰感が揃います。
		spring.Damping = m_Settings.SpringDampingRatio * 2.0f * sqrtf(stiffness * particleMass);
		spring.Structural = structural;
		m_Springs.push_back(spring);
	};

	const float structural = (std::max)(0.0f, m_Settings.Stiffness);
	const float shear = structural * (std::max)(0.0f, m_Settings.ShearRatio);
	const float bend = structural * (std::max)(0.0f, m_Settings.BendRatio);
	for (int panel = 0; panel < panels; ++panel)
	{
		for (int row = 0; row < rows; ++row)
		{
			for (int column = 0; column < columns; ++column)
			{
				const int self = index(panel, column, row);
				if (column + 1 < columns) addSpring(self, index(panel, column + 1, row), structural, true);
				if (row + 1 < rows) addSpring(self, index(panel, column, row + 1), structural, true);
				if (column + 1 < columns && row + 1 < rows)
				{
					addSpring(self, index(panel, column + 1, row + 1), shear, false);
					addSpring(index(panel, column + 1, row), index(panel, column, row + 1), shear, false);
				}
				if (bend > 0.0f)
				{
					if (column + 2 < columns) addSpring(self, index(panel, column + 2, row), bend, false);
					if (row + 2 < rows) addSpring(self, index(panel, column, row + 2), bend, false);
				}
				if (column + 1 < columns && row + 1 < rows)
				{
					// ローカル-Z側から見て時計回り(=表面)になる順です。
					const std::uint32_t a = static_cast<std::uint32_t>(self);
					const std::uint32_t b = static_cast<std::uint32_t>(index(panel, column + 1, row));
					const std::uint32_t d = static_cast<std::uint32_t>(index(panel, column, row + 1));
					const std::uint32_t e = static_cast<std::uint32_t>(index(panel, column + 1, row + 1));
					m_Triangles.insert(m_Triangles.end(), { a, b, d, b, e, d });
				}
			}
		}
	}

	XMStoreFloat4x4(&m_PinTransform, XMMatrixIdentity());
	m_Time = 0.0f;
}

void ClothSimulation::Place(const XMMATRIX& world)
{
	XMStoreFloat4x4(&m_PinTransform, world);
	for (Particle& particle : m_Particles)
	{
		particle.Position = TransformPoint(m_PinTransform, particle.LocalRest);
		particle.Velocity = {};
		particle.Force = {};
	}
}

void ClothSimulation::SetPinTransform(const XMMATRIX& world)
{
	XMStoreFloat4x4(&m_PinTransform, world);
}

void ClothSimulation::ChooseSubsteps(float deltaTime)
{
	// 半陰的オイラーはバネの固有角振動数ωに対し ω*dt が小さくないと発散します。
	// 1粒子に繋がるバネの合計から最悪ケースのωを見積もり、ω*dt <= 0.35 になるよう分割します。
	if (m_Particles.empty())
	{
		m_Substeps = 1;
		return;
	}
	float minimumMass = 1e30f;
	for (const Particle& particle : m_Particles)
	{
		if (!particle.Pinned) minimumMass = (std::min)(minimumMass, 1.0f / particle.InverseMass);
	}
	if (minimumMass >= 1e30f)
	{
		m_Substeps = 1;
		return;
	}
	const float nodeStiffness = 4.0f * m_Settings.Stiffness
		* (1.0f + m_Settings.ShearRatio + m_Settings.BendRatio);
	const float omega = sqrtf((std::max)(0.0f, nodeStiffness) / minimumMass);
	const int substeps = static_cast<int>(ceilf(deltaTime * omega / 0.35f));
	m_Substeps = std::clamp(substeps, 2, 96);
}

Vector3 ClothSimulation::SampleWind(const Vector3& position) const
{
	const Vector3& wind = m_Settings.Wind;
	if (wind.x == 0.0f && wind.y == 0.0f && wind.z == 0.0f)
	{
		return {};
	}
	// 時間と場所でずらした正弦波の和で、パネルごとに違う揺れ方をさせます。
	const float turbulence = m_Settings.WindTurbulence;
	const float gust = 1.0f + turbulence * (0.6f * sinf(1.3f * m_Time + 0.7f * position.x)
		+ 0.4f * sinf(2.9f * m_Time + 1.7f * position.x + 0.9f * position.y));
	return wind * gust;
}

void ClothSimulation::AccumulateForces()
{
	// 重力 + 空気抵抗 (授業教材の gravity + resistance)
	for (Particle& particle : m_Particles)
	{
		if (particle.Pinned)
		{
			particle.Force = {};
			continue;
		}
		const float mass = 1.0f / particle.InverseMass;
		particle.Force = m_Settings.Gravity * mass - particle.Velocity * (m_Settings.Drag * mass);
	}

	// 風と、面に垂直な方向の空気抵抗。三角形ごとに求めて3頂点へ等分します。
	// 風が無くても働くので、布がはためいた後に自然に落ち着きます。
	for (std::size_t i = 0; i + 2 < m_Triangles.size(); i += 3)
	{
		Particle& a = m_Particles[m_Triangles[i]];
		Particle& b = m_Particles[m_Triangles[i + 1]];
		Particle& c = m_Particles[m_Triangles[i + 2]];
		const Vector3 cross = Vector3::cross(b.Position - a.Position, c.Position - a.Position);
		const float doubleArea = cross.length();
		if (doubleArea <= 1e-10f)
		{
			continue;
		}
		const Vector3 normal = cross / doubleArea;
		const Vector3 centroid = (a.Position + b.Position + c.Position) / 3.0f;
		const Vector3 relative = SampleWind(centroid) - (a.Velocity + b.Velocity + c.Velocity) / 3.0f;
		const Vector3 force = normal * (m_Settings.AirCoefficient * doubleArea * 0.5f
			* Vector3::dot(relative, normal) / 3.0f);
		a.Force += force;
		b.Force += force;
		c.Force += force;
	}

	// バネ (授業教材の SPRING_COEFFICIENT * (length - 自然長)) + バネ方向の減衰
	for (const Spring& spring : m_Springs)
	{
		Particle& a = m_Particles[spring.A];
		Particle& b = m_Particles[spring.B];
		const Vector3 delta = b.Position - a.Position;
		const float length = delta.length();
		if (length < 1e-6f)
		{
			continue;
		}
		const Vector3 direction = delta / length;
		const float stretch = spring.Stiffness * (length - spring.RestLength);
		const float damping = spring.Damping * Vector3::dot(b.Velocity - a.Velocity, direction);
		const Vector3 force = direction * (stretch + damping);
		a.Force += force;
		b.Force -= force;
	}
}

void ClothSimulation::Integrate(float deltaTime)
{
	// 半陰的オイラー: 先に速度、その速度で位置 (授業教材と同じ順序)
	for (Particle& particle : m_Particles)
	{
		if (particle.Pinned)
		{
			particle.Position = TransformPoint(m_PinTransform, particle.LocalRest);
			particle.Velocity = {};
			continue;
		}
		particle.Velocity += particle.Force * (particle.InverseMass * deltaTime);
		particle.Position += particle.Velocity * deltaTime;
	}
}

void ClothSimulation::LimitStretch()
{
	const float maximum = (std::max)(1.0f, m_Settings.MaxStretch);
	for (const Spring& spring : m_Springs)
	{
		if (!spring.Structural)
		{
			continue;
		}
		Particle& a = m_Particles[spring.A];
		Particle& b = m_Particles[spring.B];
		const float totalInverseMass = a.InverseMass + b.InverseMass;
		if (totalInverseMass <= 0.0f)
		{
			continue;
		}
		const Vector3 delta = b.Position - a.Position;
		const float length = delta.length();
		const float limit = spring.RestLength * maximum;
		if (length <= limit || length < 1e-6f)
		{
			continue;
		}
		const Vector3 direction = delta / length;
		const Vector3 correction = direction * (length - limit);
		const float weightA = a.InverseMass / totalInverseMass;
		const float weightB = b.InverseMass / totalInverseMass;
		a.Position += correction * weightA;
		b.Position -= correction * weightB;
		// 位置だけ戻すと速度が残り、次の瞬間また伸びて震え続けます。
		// 離れていく向きの相対速度を消して(非弾性)、エネルギーを足さずに吸収します。
		const float separating = Vector3::dot(b.Velocity - a.Velocity, direction);
		if (separating > 0.0f)
		{
			a.Velocity += direction * (separating * weightA);
			b.Velocity -= direction * (separating * weightB);
		}
	}
}

void ClothSimulation::Collide(const std::vector<ClothCollisionShape>& shapes, float blend, float frameDeltaTime)
{
	if (shapes.empty())
	{
		return;
	}
	const float thickness = (std::max)(0.0f, m_Settings.CollisionThickness);
	// 摩擦は1/60秒あたりの減衰率として扱い、サブステップ数に依存しないようにします。
	const float substepDelta = frameDeltaTime / static_cast<float>(m_Substeps);
	const float keepTangent = powf(std::clamp(1.0f - m_Settings.Friction, 0.0f, 1.0f), substepDelta * 60.0f);

	for (const ClothCollisionShape& shape : shapes)
	{
		const Vector3 center = Lerp(shape.PreviousCenter, shape.Center, blend);
		const Vector3 shapeVelocity = frameDeltaTime > 0.0f
			? (shape.Center - shape.PreviousCenter) / frameDeltaTime
			: Vector3{};

		for (Particle& particle : m_Particles)
		{
			if (particle.Pinned)
			{
				continue;
			}

			Vector3 normal;
			bool inside = false;
			if (shape.Type == Collider::Shape::AABB)
			{
				const Vector3 local = particle.Position - center;
				const Vector3 half = shape.HalfSize + Vector3{ thickness, thickness, thickness };
				const float depthX = half.x - fabsf(local.x);
				const float depthY = half.y - fabsf(local.y);
				const float depthZ = half.z - fabsf(local.z);
				if (depthX > 0.0f && depthY > 0.0f && depthZ > 0.0f)
				{
					inside = true;
					// 一番浅い軸へ押し出します。
					if (depthX <= depthY && depthX <= depthZ)
					{
						normal = { local.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f };
						particle.Position.x += normal.x * depthX;
					}
					else if (depthY <= depthZ)
					{
						normal = { 0.0f, local.y >= 0.0f ? 1.0f : -1.0f, 0.0f };
						particle.Position.y += normal.y * depthY;
					}
					else
					{
						normal = { 0.0f, 0.0f, local.z >= 0.0f ? 1.0f : -1.0f };
						particle.Position.z += normal.z * depthZ;
					}
				}
			}
			else
			{
				// 球はカプセルの軸が点になったものとして同じ処理にします。
				Vector3 closest = center;
				if (shape.Type == Collider::Shape::Capsule)
				{
					const Vector3 axis{ 0.0f, shape.CapsuleHalfHeight, 0.0f };
					closest = ClosestPointOnSegment(particle.Position, center - axis, center + axis);
				}
				const float radius = shape.Radius + thickness;
				const Vector3 offset = particle.Position - closest;
				const float distance = offset.length();
				if (distance < radius)
				{
					inside = true;
					normal = distance > 1e-6f ? offset / distance : Vector3{ 0.0f, 0.0f, -1.0f };
					particle.Position = closest + normal * radius;
				}
			}

			if (!inside)
			{
				continue;
			}
			// 相手に対する相対速度で、めり込む向きを消し、横滑りを摩擦で弱めます。
			// これで布が体に沿って引きずられ、離れると重力で戻ります。
			const Vector3 relative = particle.Velocity - shapeVelocity;
			const float normalSpeed = Vector3::dot(relative, normal);
			const Vector3 normalPart = normal * (std::max)(0.0f, normalSpeed);
			const Vector3 tangentPart = (relative - normal * normalSpeed) * keepTangent;
			particle.Velocity = shapeVelocity + normalPart + tangentPart;
		}
	}
}

void ClothSimulation::Step(float deltaTime, const std::vector<ClothCollisionShape>& shapes)
{
	if (deltaTime <= 0.0f || m_Particles.empty())
	{
		return;
	}
	// 処理落ちで大きなdtが来ても、1フレームで布が飛ばないよう上限を設けます。
	deltaTime = (std::min)(deltaTime, 1.0f / 20.0f);
	ChooseSubsteps(deltaTime);
	const float substepDelta = deltaTime / static_cast<float>(m_Substeps);
	for (int substep = 0; substep < m_Substeps; ++substep)
	{
		AccumulateForces();
		Integrate(substepDelta);
		LimitStretch();
		Collide(shapes, static_cast<float>(substep + 1) / static_cast<float>(m_Substeps), deltaTime);
		m_Time += substepDelta;
	}
}

bool ClothSimulation::IsFinite() const
{
	for (const Particle& particle : m_Particles)
	{
		if (!IsFiniteVector(particle.Position) || !IsFiniteVector(particle.Velocity))
		{
			return false;
		}
	}
	return true;
}

float ClothSimulation::GetMaxStructuralStretch() const
{
	float maximum = 0.0f;
	for (const Spring& spring : m_Springs)
	{
		if (!spring.Structural || spring.RestLength <= 0.0f) continue;
		const float length = (m_Particles[spring.B].Position - m_Particles[spring.A].Position).length();
		maximum = (std::max)(maximum, length / spring.RestLength);
	}
	return maximum;
}

float ClothSimulation::GetMaxSpeed() const
{
	float maximum = 0.0f;
	for (const Particle& particle : m_Particles)
	{
		maximum = (std::max)(maximum, particle.Velocity.length());
	}
	return maximum;
}

Vector3 ClothSimulation::GetRestPosition(const Particle& particle) const
{
	return TransformPoint(m_PinTransform, particle.LocalRest);
}

float ClothSimulation::GetMaxDisplacementFromRest() const
{
	float maximum = 0.0f;
	for (const Particle& particle : m_Particles)
	{
		maximum = (std::max)(maximum, (particle.Position - GetRestPosition(particle)).length());
	}
	return maximum;
}

void ClothSimulation::GetBounds(Vector3& minimum, Vector3& maximum) const
{
	if (m_Particles.empty())
	{
		minimum = maximum = {};
		return;
	}
	minimum = maximum = m_Particles.front().Position;
	for (const Particle& particle : m_Particles)
	{
		minimum = { (std::min)(minimum.x, particle.Position.x), (std::min)(minimum.y, particle.Position.y),
			(std::min)(minimum.z, particle.Position.z) };
		maximum = { (std::max)(maximum.x, particle.Position.x), (std::max)(maximum.y, particle.Position.y),
			(std::max)(maximum.z, particle.Position.z) };
	}
}

//----------------------------------------------------------------------------
// Cloth
//----------------------------------------------------------------------------

Cloth::Cloth(const ClothSettings& settings)
{
	m_Simulation.Build(settings);
}

XMMATRIX Cloth::GetPinMatrix() const
{
	return XMMatrixRotationRollPitchYaw(GetRotation().x, GetRotation().y, GetRotation().z)
		* XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);
}

void Cloth::Init()
{
	m_Layer = Layer::World;
	m_Simulation.Place(GetPinMatrix());
	m_Placed = false;
	CreateGpuResources();
}

void Cloth::Uninit()
{
	m_VertexBuffer.Reset();
	m_IndexBuffer.Reset();
	m_PreviousCenters.clear();
}

void Cloth::CreateGpuResources()
{
	const auto& particles = m_Simulation.GetParticles();
	const auto& triangles = m_Simulation.GetTriangles();
	if (particles.empty() || triangles.empty())
	{
		m_RenderingAvailable = false;
		return;
	}

	// 表と裏で法線を逆にするため、頂点は2組持ちます。裏面は巻き順も逆にします。
	D3D11_BUFFER_DESC vertexDescription{};
	vertexDescription.Usage = D3D11_USAGE_DYNAMIC;
	vertexDescription.ByteWidth = static_cast<UINT>(particles.size() * 2 * sizeof(VERTEX_3D));
	vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	m_RenderingAvailable = GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, vertexDescription, nullptr, "Cloth");

	const std::uint32_t backOffset = static_cast<std::uint32_t>(particles.size());
	std::vector<std::uint32_t> indices(triangles);
	indices.reserve(triangles.size() * 2);
	for (std::size_t i = 0; i + 2 < triangles.size(); i += 3)
	{
		indices.insert(indices.end(), {
			triangles[i] + backOffset, triangles[i + 2] + backOffset, triangles[i + 1] + backOffset });
	}
	D3D11_BUFFER_DESC indexDescription{};
	indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
	indexDescription.ByteWidth = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));
	indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();
	m_RenderingAvailable = GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, indexDescription, &indexData, "ClothIndices") && m_RenderingAvailable;
	m_IndexCount = static_cast<UINT>(indices.size());

	m_RenderingAvailable = m_Surface.Load(SurfaceShader::Directional, m_Texture) && m_RenderingAvailable;
}

void Cloth::SetTexture(TextureHandle texture)
{
	m_Texture = std::move(texture);
	m_Surface.Texture = m_Texture;
	m_Surface.Properties.TextureEnable = m_Texture != nullptr;
}

bool Cloth::SetShaderDefinition(std::string_view id)
{
	return m_Surface.Load(id, m_Texture);
}

void Cloth::SetWind(const Vector3& velocity, float turbulence)
{
	ClothSettings& settings = m_Simulation.GetSettings();
	settings.Wind = velocity;
	settings.WindTurbulence = turbulence;
}

void Cloth::ResetToRest()
{
	m_Simulation.Place(GetPinMatrix());
	m_PreviousCenters.clear();
	m_Placed = true;
}

void Cloth::GatherShapes()
{
	m_ShapeScratch.clear();
	m_QueryScratch.clear();
	if (m_World == nullptr)
	{
		return;
	}

	Vector3 minimum;
	Vector3 maximum;
	m_Simulation.GetBounds(minimum, maximum);
	// 1フレームで動ける距離ぶん広げて、入ってくる直前の物体も拾います。
	const Vector3 margin{ 0.5f, 0.5f, 0.5f };
	GetWorld().GetCollisionWorld().QueryBounds(minimum - margin, maximum + margin,
		m_Simulation.GetSettings().CollisionMask, m_QueryScratch);

	std::unordered_map<std::uint64_t, Vector3> currentCenters;
	currentCenters.reserve(m_QueryScratch.size());
	for (const Collider* collider : m_QueryScratch)
	{
		if (collider->GetOwner() == this)
		{
			continue;
		}
		ClothCollisionShape shape;
		shape.Type = collider->GetShape();
		shape.Center = collider->GetCenter();
		shape.Radius = collider->GetRadius();
		shape.CapsuleHalfHeight = collider->GetCapsuleHalfHeight();
		shape.HalfSize = collider->GetHalfSize();
		// 登録シリアルで前フレームの位置を引きます。ポインタと違い、破棄後に再利用されません。
		const auto previous = m_PreviousCenters.find(collider->GetRegistrationSerial());
		shape.PreviousCenter = previous != m_PreviousCenters.end() ? previous->second : shape.Center;
		currentCenters.emplace(collider->GetRegistrationSerial(), shape.Center);
		m_ShapeScratch.push_back(shape);
	}
	m_PreviousCenters = std::move(currentCenters);
}

void Cloth::Simulate(float deltaTime)
{
	if (!m_Placed)
	{
		// Spawn直後にSetPositionされても、最初の更新で置き直すので引きずられません。
		ResetToRest();
	}
	m_Simulation.SetPinTransform(GetPinMatrix());
	GatherShapes();
	m_Simulation.Step(deltaTime, m_ShapeScratch);
}

void Cloth::LateUpdate()
{
	// 他のオブジェクトがUpdateで動き終えた後の位置で押し出すため、LateUpdateで進めます。
	Simulate(GameTime::GetDeltaTime());
}

void Cloth::Draw()
{
	if (!UploadVertices())
	{
		return;
	}
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	m_Surface.Bind();
	SubmitGeometry();
}

void Cloth::DrawShadow()
{
	// 影パスは通常の描画より先に走るので、ここでも最新の粒子位置を書いてから描きます。
	if (!UploadVertices())
	{
		return;
	}
	ShadowMap::BindCaster(false);
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	SubmitGeometry();
}

void Cloth::SubmitGeometry()
{
	auto* context = Renderer::GetDeviceContext();
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_IndexCount, 0, 0);
}

bool Cloth::UploadVertices()
{
	if (!m_Placed)
	{
		ResetToRest();
	}
	const auto& particles = m_Simulation.GetParticles();
	const auto& triangles = m_Simulation.GetTriangles();

	m_NormalScratch.assign(particles.size(), Vector3{});
	for (std::size_t i = 0; i + 2 < triangles.size(); i += 3)
	{
		const Vector3& a = particles[triangles[i]].Position;
		const Vector3& b = particles[triangles[i + 1]].Position;
		const Vector3& c = particles[triangles[i + 2]].Position;
		// 時計回りの三角形では、この外積が表側(見ている側)を向きます。面積で重み付けされます。
		const Vector3 faceNormal = Vector3::cross(b - a, c - a);
		m_NormalScratch[triangles[i]] += faceNormal;
		m_NormalScratch[triangles[i + 1]] += faceNormal;
		m_NormalScratch[triangles[i + 2]] += faceNormal;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (!GraphicsResourceFactory::MapWriteDiscard(m_VertexBuffer.Get(), mapped, "Cloth"))
	{
		return false;
	}
	VERTEX_3D* vertices = static_cast<VERTEX_3D*>(mapped.pData);
	const std::size_t count = particles.size();
	for (std::size_t i = 0; i < count; ++i)
	{
		Vector3 normal = m_NormalScratch[i];
		normal.normalize();
		const Vector3& position = particles[i].Position;
		VERTEX_3D& front = vertices[i];
		front.Position = { position.x, position.y, position.z };
		front.Normal = { normal.x, normal.y, normal.z };
		front.Diffuse = m_Color;
		front.TexCoord = { particles[i].U, particles[i].V };
		VERTEX_3D& back = vertices[i + count];
		back = front;
		back.Normal = { -normal.x, -normal.y, -normal.z };
	}
	Renderer::GetDeviceContext()->Unmap(m_VertexBuffer.Get(), 0);
	return true;
}
}
