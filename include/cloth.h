#pragma once

#include "gameObject.h"
#include "collider.h"
#include "collisionWorld.h"
#include "resourceManager.h"
#include "surfaceMaterial.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

// バネ質量モデルの布です。GY「布」の授業教材(粒子+バネ+重力+抵抗+風、半陰的オイラー)を
// 3Dのメートル単位へ移し、次の点を足しています。
//   - 曲げバネ(1つ飛ばしのバネ)とバネ方向の減衰
//   - サブステップ数をバネの硬さから自動決定(分割数や質量を変えても発散しない)
//   - 伸びすぎの制限(引っ張られてもゴムのように伸びない)
//   - 球・カプセル・AABBのColliderとの押し出しと摩擦
// 布自身はColliderを持ちません。周囲の物体に押されるだけの一方向の当たり判定です。
namespace Engine
{
struct ClothSettings
{
	// 横に並べる布の枚数です。のれんの切れ目は2以上、一枚布なら1にします。
	int PanelCount = 1;
	// 1枚あたりの横方向の粒子数と、縦方向の粒子数です(どちらも2以上)。
	int ColumnsPerPanel = 8;
	int Rows = 10;
	// 布全体の幅と丈[m]、切れ目の隙間[m]です。
	float Width = 1.0f;
	float Height = 1.0f;
	float PanelGap = 0.0f;
	// 布全体の質量[kg]です。粒子へ均等に割り振ります。
	float Mass = 0.12f;

	// 構造バネ(上下左右)のバネ係数[N/m]です。せん断・曲げはこれに比率を掛けます。
	// 自重で上端のバネが伸びる割合はおよそ 質量*9.8 / (横の粒子数 * バネ係数 * 縦の粒子間隔) です。
	// これがMaxStretchを超えると常に伸び制限頼みになるので、布を重く・粗くしたら硬くしてください。
	float Stiffness = 80.0f;
	float ShearRatio = 0.5f;
	float BendRatio = 0.15f;
	// バネ方向の減衰を臨界減衰の何割にするかです。振動の収まり方を決めます。
	float SpringDampingRatio = 0.08f;
	// 空気抵抗(速度に比例する減速)[1/s]です。授業教材のATT_COEFFICIENTに当たります。
	float Drag = 1.0f;
	// 構造バネを自然長の何倍まで伸ばしてよいかです。
	float MaxStretch = 1.04f;

	Vector3 Gravity{ 0.0f, -9.8f, 0.0f };
	// 風の速度[m/s]と、時間・場所による揺らぎの強さ(0で一定)です。
	Vector3 Wind{ 0.0f, 0.0f, 0.0f };
	float WindTurbulence = 0.5f;
	// 面に垂直な相対風速に比例する空気力の係数です。大きいほど風にあおられます。
	float AirCoefficient = 0.9f;

	// Colliderの表面から粒子を離す距離[m]と、接触中の横滑りの減衰(0-1)です。
	float CollisionThickness = 0.02f;
	float Friction = 0.35f;
	// 押されて反応する相手のカテゴリです。
	CollisionLayer CollisionMask = CollisionLayers::All;
};

// Colliderから取り出した、1ステップ分の形状です。前フレームの位置も持ち、
// サブステップの間を補間して速い物体でもすり抜けにくくします。
struct ClothCollisionShape
{
	Collider::Shape Type = Collider::Shape::Sphere;
	Vector3 Center;
	Vector3 PreviousCenter;
	float Radius = 0.0f;
	float CapsuleHalfHeight = 0.0f;
	Vector3 HalfSize;
};

// 描画に依存しない布の物理です。単体でステップを進められるので自己テストで検証できます。
class ClothSimulation
{
public:
	struct Particle
	{
		Vector3 Position;
		Vector3 Velocity;
		Vector3 Force;
		// 布のローカル座標での初期位置です。固定点の追従と復元確認に使います。
		Vector3 LocalRest;
		float InverseMass = 0.0f;
		float U = 0.0f;
		float V = 0.0f;
		bool Pinned = false;
	};

	struct Spring
	{
		int A = 0;
		int B = 0;
		float RestLength = 0.0f;
		float Stiffness = 0.0f;
		float Damping = 0.0f;
		bool Structural = false;
	};

	void Build(const ClothSettings& settings);
	// 全粒子を初期形状のままワールドへ置き直し、速度を0にします。
	void Place(const XMMATRIX& world);
	// 固定点(上端)の追従先です。布を動く物に付ける場合は毎フレーム更新します。
	void SetPinTransform(const XMMATRIX& world);
	void Step(float deltaTime, const std::vector<ClothCollisionShape>& shapes);

	const ClothSettings& GetSettings() const { return m_Settings; }
	ClothSettings& GetSettings() { return m_Settings; }
	const std::vector<Particle>& GetParticles() const { return m_Particles; }
	const std::vector<Spring>& GetSprings() const { return m_Springs; }
	// 描画用の三角形です(時計回りが表面)。
	const std::vector<std::uint32_t>& GetTriangles() const { return m_Triangles; }
	int GetSubsteps() const { return m_Substeps; }

	// 以下は検証用です。
	bool IsFinite() const;
	float GetMaxStructuralStretch() const;
	float GetMaxSpeed() const;
	// 初期形状(固定点の変換を適用した位置)からの最大のずれ[m]です。
	float GetMaxDisplacementFromRest() const;
	Vector3 GetRestPosition(const Particle& particle) const;
	void GetBounds(Vector3& minimum, Vector3& maximum) const;

private:
	ClothSettings m_Settings;
	std::vector<Particle> m_Particles;
	std::vector<Spring> m_Springs;
	std::vector<std::uint32_t> m_Triangles;
	XMFLOAT4X4 m_PinTransform{};
	int m_Substeps = 8;
	float m_Time = 0.0f;

	void ChooseSubsteps(float deltaTime);
	void AccumulateForces();
	void Integrate(float deltaTime);
	void LimitStretch();
	void Collide(const std::vector<ClothCollisionShape>& shapes, float blend, float frameDeltaTime);
	Vector3 SampleWind(const Vector3& position) const;
};

// ワールドへ置ける布のオブジェクトです。上端の中央がGameObjectの位置で、
// 上端はローカル+X方向へ伸び、布は-Y方向へ垂れ下がります。表面はローカル-Z向きです。
class Cloth : public GameObject
{
public:
	explicit Cloth(const ClothSettings& settings = {});

	void Init() override;
	void Uninit() override;
	void LateUpdate() override;
	void Draw() override;
	void DrawShadow() override;

	// 周囲のColliderを集めて、指定時間だけ布を進めます。通常はLateUpdateから呼ばれます。
	void Simulate(float deltaTime);
	// 初期形状へ戻します。位置や向きを変えた直後に呼ぶと、瞬間移動として扱います。
	void ResetToRest();

	void SetTexture(TextureHandle texture);
	// 登録済みの表面シェーダーへ切り替えます(ModelRenderer / AnimationModel と同じ使い方)。
	bool SetShaderDefinition(std::string_view id);
	bool SetShaderFloat(std::string_view name, float value) { return m_Surface.SetFloat(name, value); }
	void SetColor(const XMFLOAT4& color) { m_Color = color; }
	void SetWind(const Vector3& velocity, float turbulence);
	void SetCollisionMask(CollisionLayer mask) { m_Simulation.GetSettings().CollisionMask = mask; }

	const ClothSimulation& GetSimulation() const { return m_Simulation; }

private:
	ClothSimulation m_Simulation;
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	UINT m_IndexCount = 0;
	SurfaceMaterial m_Surface;
	TextureHandle m_Texture;
	XMFLOAT4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	// Spawn直後はまだ呼び出し側がSetPositionしていないため、最初の更新か描画で置き直します。
	bool m_Placed = false;
	// Colliderの登録シリアルごとの前フレーム中心です。速度推定と補間に使います。
	std::unordered_map<std::uint64_t, Vector3> m_PreviousCenters;
	std::vector<Collider*> m_QueryScratch;
	std::vector<ClothCollisionShape> m_ShapeScratch;
	std::vector<Vector3> m_NormalScratch;

	XMMATRIX GetPinMatrix() const;
	void GatherShapes();
	void CreateGpuResources();
	// 現在の粒子位置と法線を頂点バッファへ書きます。影パスと通常描画の両方が使います。
	bool UploadVertices();
	void SubmitGeometry();
};
}

using ClothSettings = Engine::ClothSettings;
using Cloth = Engine::Cloth;
