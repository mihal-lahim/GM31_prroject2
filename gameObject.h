#pragma once
#include "main.h"

#include "component.h"

class GameObject
{
protected:

	bool m_Destroy = false;
	int m_Layer = 1;
	Vector3 m_Position{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Rotation{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Scale{ 1.0f, 1.0f, 1.0f };

	 std::list<Component*> m_Components;
public:

	int GetLayer() const { return m_Layer; }

	void SetPosition(const Vector3& Position) { m_Position = Position; }
	void SetRotation(const Vector3& Rotation) { m_Rotation = Rotation; }
	void SetScale(const Vector3& Scale) { m_Scale = Scale; }

	Vector3 GetPosition() const { return m_Position; }
	Vector3 GetRotation() const { return m_Rotation; }
	Vector3 GetScale() const { return m_Scale; }

	void SetDestroy() { m_Destroy = true; }
	bool Destroy()
	{
		if(m_Destroy)
		{ 
			Uninit();
			delete this;
			return true;
		}
		else
		{
			return false;
		}
	}

	virtual void Init() {}
	virtual void Uninit() {
		for (Component* component : m_Components)
		{
			component->Uninit();
			delete component;
		}
	}
	virtual void Update() {
		for (Component* component : m_Components)
		{
			component->Update();
		}
	}
	virtual void Draw() {
		for (Component* component : m_Components)
		{
			component->Draw();
		}
	}

	template<typename T>//テンプレート関数
	T* AddComponent(GameObject* Object)
	{
		T* component = new T(Object);
		component->Init();
		m_Components.push_back(component);
		return component;
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
