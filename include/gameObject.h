#pragma once
#include "main.h"

#include "component.h"

enum class Layer
{
	Camera = 0,
	World = 1,
	Effect = 2,
	UI = 3,
	Count
};

class GameObject
{
protected:

	bool m_Destroy = false;
	bool m_Uninitialized = false;
	Layer m_Layer = Layer::World;

	float m_CameraZ = 0.0f; // ソート用のカメラ奥行き値

	Vector3 m_Position{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Rotation{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Scale{ 1.0f, 1.0f, 1.0f };

	std::list<std::unique_ptr<Component>> m_Components;

	// Managerからの明示破棄とデストラクタの両方で呼ばれても、一度だけ後片付けする。
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
public:
	virtual ~GameObject() { GameObject::Uninit(); }

	Layer GetLayer() const { return m_Layer; }

	float GetCameraZ() const { return m_CameraZ; }
	void CalcCameraZ(const Vector3& CameraPosition, const Vector3& CameraForward)
	{
		Vector3 direction = m_Position - CameraPosition;
		m_CameraZ = Vector3::dot(direction, CameraForward);
	}

	void SetPosition(const Vector3& Position) { m_Position = Position; }
	void SetRotation(const Vector3& Rotation) { m_Rotation = Rotation; }
	void SetScale(const Vector3& Scale) { m_Scale = Scale; }

	Vector3 GetPosition() const { return m_Position; }
	Vector3 GetRotation() const { return m_Rotation; }
	Vector3 GetScale() const { return m_Scale; }

	void SetDestroy() { m_Destroy = true; }
	bool Destroy()
	{
		return m_Destroy;
	}

	virtual void Init() {}
	virtual void Uninit() {
		if (!BeginUninit())
		{
			return;
		}

		UninitComponents();
	}
	virtual void Update() {
		for (auto& component : m_Components)
		{
			component->Update();
		}
	}
	virtual void Draw() {
		for (auto& component : m_Components)
		{
			component->Draw();
		}
	}

	template<typename T>// テンプレート関数
	T* AddComponent(GameObject* Object)
	{
		auto component = std::make_unique<T>(Object);
		T* componentPtr = component.get();
		component->Init();
		m_Components.push_back(std::move(component));
		m_Uninitialized = false;
		return componentPtr;
	}
	virtual Vector3 GetForward()
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, 
													m_Rotation.y, m_Rotation.z);
		Vector3 forward;
		XMStoreFloat3((XMFLOAT3*)&forward, rot.r[2]);
		return forward;
	}
	virtual Vector3 GetRight()
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x,
			m_Rotation.y, m_Rotation.z);
		Vector3 right;
		XMStoreFloat3((XMFLOAT3*)&right, rot.r[0]);
		return right;
	}
	virtual Vector3 GetUp()
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x,
			m_Rotation.y, m_Rotation.z);
		Vector3 up;
		XMStoreFloat3((XMFLOAT3*)&up, rot.r[1]);
		return up;
	}
	virtual Vector3 GetLeft() 
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x,
			m_Rotation.y, m_Rotation.z);
		Vector3 left;
		XMStoreFloat3((XMFLOAT3*)&left, rot.r[0]);
		return -left;
	}
};


