#pragma once
#include "engineCommon.h"

#include "component.h"
#include <type_traits>
#include <utility>

class World;

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
	Layer m_Layer = Layer::World;
	RenderCategory m_RenderCategory = RenderCategory::Auto;

	float m_CameraZ = 0.0f; // ソート用のカメラ奥行き値

	Vector3 m_Position{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Rotation{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Scale{ 1.0f, 1.0f, 1.0f };

	std::list<std::unique_ptr<Component>> m_Components;
	World* m_World = nullptr;

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

	void SetDestroy() { m_Destroy = true; }
	bool Destroy()
	{
		return m_Destroy;
	}

	// 非virtualの実行入口です。派生クラスが基底実装を呼び忘れても
	// Componentの更新・破棄が欠落しないようにします。
	void Initialize()
	{
		m_Uninitialized = false;
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
		Draw();
		for (auto& component : m_Components)
		{
			component->Draw();
		}
	}

	virtual void Init() {}
	virtual void Uninit() {}
	virtual void PreUpdate() {}
	virtual void Update() {}
	virtual void LateUpdate() {}
	virtual void Draw() {}

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args)
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component.");
		auto component = std::make_unique<T>(this, std::forward<Args>(args)...);
		T* componentPtr = component.get();
		m_Components.push_back(std::move(component));
		m_Uninitialized = false;
		componentPtr->Init();
		return componentPtr;
	}
	virtual Vector3 GetForward()
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, 
											m_Rotation.y, m_Rotation.z);
		XMFLOAT3 forward;
		XMStoreFloat3(&forward, rot.r[2]);
		return { forward.x, forward.y, forward.z };
	}
	virtual Vector3 GetRight()
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x,
			m_Rotation.y, m_Rotation.z);
		XMFLOAT3 right;
		XMStoreFloat3(&right, rot.r[0]);
		return { right.x, right.y, right.z };
	}
	virtual Vector3 GetUp()
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x,
			m_Rotation.y, m_Rotation.z);
		XMFLOAT3 up;
		XMStoreFloat3(&up, rot.r[1]);
		return { up.x, up.y, up.z };
	}
	virtual Vector3 GetLeft() 
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x,
			m_Rotation.y, m_Rotation.z);
		XMFLOAT3 left;
		XMStoreFloat3(&left, rot.r[0]);
		return { -left.x, -left.y, -left.z };
	}
};


