#pragma once
#include "engineCommon.h"

#include "component.h"
#include <cfloat>
#include <type_traits>
#include <utility>

class World;
namespace Engine { class PropertyVisitor; }

enum class Layer
{
	Camera = 0,
	World = 1,
	Effect = 2,
	UI = 3,
	Count
};

enum class RenderCategory
{
	Auto,
	Opaque,
	Transparent,
	Overlay
};

// Worldが所有するゲーム要素の基底クラスです。
// 派生クラスは仮想関数に固有処理だけを書き、コンポーネント実行はRun*入口に任せます。
class GameObject
{
	friend class World;

protected:

	bool m_Destroy = false;
	bool m_Uninitialized = false;
	// GPUリソースを用意できなかった場合も、更新や当たり判定は継続し、描画だけを止めます。
	bool m_RenderingAvailable = true;
	Layer m_Layer = Layer::World;
	RenderCategory m_RenderCategory = RenderCategory::Auto;

	float m_CameraZ = 0.0f; // ソート用のカメラ奥行き値


	Vector3 m_Rotation{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Scale{ 1.0f, 1.0f, 1.0f };

	std::list<std::unique_ptr<Component>> m_Components;
	World* m_World = nullptr;
	// World内で重複しない番号です。Spawn時にWorldが振り、シーンファイルにも保存されます。
	std::uint64_t m_Id = 0;
	// エディタの階層表示とシーンファイルで使う表示名です。空でも構いません。
	std::string m_Name;

	// Spawn直後にWorldが一度だけ設定します。ゲーム側からの差し替えは禁止です。
	void AttachWorld(World* world) { m_World = world; }

	// Worldからの明示破棄とデストラクタの両方で呼ばれても、一度だけ後片付けする。
	bool BeginUninit()
	{
		if (m_Uninitialized)
		{
			return false;
		}

		m_Uninitialized = true;
		return true;
	}

	void UninitComponents()
	{
		for (auto& component : m_Components)
		{
			component->Uninit();
		}
		m_Components.clear();
	}
	// 静的コライダーの空間インデックスなど、変換に依存するComponentへ通知します。
	void NotifyTransformChanged()
	{
		for (auto& component : m_Components)
		{
			component->OnOwnerTransformChanged();
		}
	}
public:
	virtual ~GameObject() { Shutdown(); }

	Layer GetLayer() const { return m_Layer; }
	void SetRenderCategory(RenderCategory category) { m_RenderCategory = category; }
	RenderCategory GetRenderCategory() const
	{
		if (m_RenderCategory != RenderCategory::Auto)
		{
			return m_RenderCategory;
		}
		if (m_Layer == Layer::Effect)
		{
			return RenderCategory::Transparent;
		}
		if (m_Layer == Layer::UI)
		{
			return RenderCategory::Overlay;
		}
		return RenderCategory::Opaque;
	}
	World& GetWorld()
	{
		assert(m_World != nullptr);
		return *m_World;
	}
	const World& GetWorld() const
	{
		assert(m_World != nullptr);
		return *m_World;
	}

	float GetCameraZ() const { return m_CameraZ; }
	void CalcCameraZ(const Vector3& CameraPosition, const Vector3& CameraForward)
	{
		Vector3 direction = m_Position - CameraPosition;
		m_CameraZ = Vector3::dot(direction, CameraForward);
	}

	void SetPosition(const Vector3& Position) { m_Position = Position; NotifyTransformChanged(); }
	void SetRotation(const Vector3& Rotation) { m_Rotation = Rotation; NotifyTransformChanged(); }
	void SetScale(const Vector3& Scale) { m_Scale = Scale; NotifyTransformChanged(); }

	Vector3 GetPosition() const { return m_Position; }
	Vector3 GetRotation() const { return m_Rotation; }
	Vector3 GetScale() const { return m_Scale; }

	std::uint64_t GetId() const { return m_Id; }
	const std::string& GetName() const { return m_Name; }
	void SetName(std::string name) { m_Name = std::move(name); }
	const std::list<std::unique_ptr<Component>>& GetComponents() const { return m_Components; }

	// 保存・読込・インスペクタ表示の対象にしたい値を、visitorへ1つずつ渡します。
	// 位置・回転・スケールと名前は共通で扱われるので、ここへ書く必要はありません。
	virtual void Reflect(Engine::PropertyVisitor& visitor) { (void)visitor; }
	// シーンファイルから読み込んだ直後と、エディタが位置や値を書き換えた後に呼ばれます。
	// Reflectで受け取った値を、モデルの切り替えなど実際の状態へ反映する場所です。
	virtual void OnPropertiesChanged() {}

	void MarkForDestroy() { m_Destroy = true; }
	bool IsPendingDestroy() const { return m_Destroy; }

	// 非virtualの実行入口です。派生クラスが基底実装を呼び忘れても
	// Componentの更新・破棄が欠落しないようにします。
	void Initialize()
	{
		m_Uninitialized = false;
		m_RenderingAvailable = true;
		Init();
	}
	void Shutdown()
	{
		if (!BeginUninit())
		{
			return;
		}
		Uninit();
		UninitComponents();
	}
	void RunPreUpdate()
	{
		PreUpdate();
		for (auto& component : m_Components)
		{
			component->PreUpdate();
		}
	}
	void RunUpdate()
	{
		Update();
		for (auto& component : m_Components)
		{
			component->Update();
		}
	}
	void RunLateUpdate()
	{
		LateUpdate();
		for (auto& component : m_Components)
		{
			component->LateUpdate();
		}
	}
	void RunDraw()
	{
		if (m_RenderingAvailable)
		{
			Draw();
		}
		for (auto& component : m_Components)
		{
			component->Draw();
		}
	}

	// 影パスの入口です。RenderQueueが通常の描画より前に、太陽から見た深度を描くために呼びます。
	void RunDrawShadow()
	{
		if (m_RenderingAvailable)
		{
			DrawShadow();
		}
		for (auto& component : m_Components)
		{
			component->DrawShadow();
		}
	}

	virtual void Init() {}
	virtual void Uninit() {}
	virtual void PreUpdate() {}
	virtual void Update() {}
	virtual void LateUpdate() {}
	virtual void Draw() {}
	// 影を落とす場合だけ実装します。Engine::ShadowMap::BindCaster を呼んでから形状を描きます。
	virtual void DrawShadow() {}

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component.");
		// Shutdown済みのオブジェクトへ追加すると、二重Uninit防止の不変条件が崩れます。
		assert(!m_Uninitialized && "Shutdown済みのGameObjectへAddComponentしています");
		auto component = std::make_unique<T>(this, std::forward<Args>(args)...);
		T* componentPtr = component.get();
		m_Components.push_back(std::move(component));
		componentPtr->Init();
		return componentPtr;
	}
	// m_Rotationはprotectedで、Setterを通さず直接書き換える実装が多数あります。
	// そのためダーティフラグではなく「回転の値そのもの」をキャッシュのキーにします。
	// float3個の比較で済み、直接書き換えに対しても常に正しい向きを返します。
	virtual Vector3 GetForward() { return RefreshAxes().Forward; }
	virtual Vector3 GetRight() { return RefreshAxes().Right; }
	virtual Vector3 GetUp() { return RefreshAxes().Up; }
	virtual Vector3 GetLeft()
	{
		const Vector3 right = RefreshAxes().Right;
		return { -right.x, -right.y, -right.z };
	}

private:
	struct RotationAxes
	{
		Vector3 Right{ 1.0f, 0.0f, 0.0f };
		Vector3 Up{ 0.0f, 1.0f, 0.0f };
		Vector3 Forward{ 0.0f, 0.0f, 1.0f };
	};

	// 初回は必ずミスするよう、あり得ない回転値で初期化します。
	mutable Vector3 m_CachedAxesRotation{ FLT_MAX, FLT_MAX, FLT_MAX };
	// 直接書き換えると当たり判定のインデックスが古くなるため、派生クラスにも見せません。
	// 書き込みは必ずSetPositionを通します。
	Vector3 m_Position{ 0.0f, 0.0f, 0.0f };
	mutable RotationAxes m_CachedAxes;

	const RotationAxes& RefreshAxes() const
	{
		if (m_CachedAxesRotation.x == m_Rotation.x
			&& m_CachedAxesRotation.y == m_Rotation.y
			&& m_CachedAxesRotation.z == m_Rotation.z)
		{
			return m_CachedAxes;
		}

		const XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
		XMFLOAT3 axis;
		XMStoreFloat3(&axis, rot.r[0]);
		m_CachedAxes.Right = { axis.x, axis.y, axis.z };
		XMStoreFloat3(&axis, rot.r[1]);
		m_CachedAxes.Up = { axis.x, axis.y, axis.z };
		XMStoreFloat3(&axis, rot.r[2]);
		m_CachedAxes.Forward = { axis.x, axis.y, axis.z };
		m_CachedAxesRotation = m_Rotation;
		return m_CachedAxes;
	}
};


