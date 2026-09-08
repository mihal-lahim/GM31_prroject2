#pragma once

#include "gameObject.h"
#include "renderer.h"
#include "wireBreakable.h"
#include "wireTarget.h"

// 作業員を閉じ込めているロック機構(扉・拘束具)です。
//
// 剣で壊すことで救助ルートが開きます。ワイヤーの掛かり所も兼ねるので、
// 「高速で近づいて斬り、そのまま次へ抜ける」動線に組み込めます。
class WireLockGate : public GameObject, public IWireTarget, public IWireBreakable
{
private:
	Vector3 m_HalfSize{ 3.0f, 4.0f, 1.0f };
	int m_Health = 180;
	int m_MaxHealth = 180;
	bool m_Broken = false;
	float m_HitFlash = 0.0f;

	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	unsigned int m_IndexCount = 0;

	void BuildMesh();

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	void Setup(const Vector3& position, const Vector3& halfSize, int health);
	bool IsBroken() const { return m_Broken; }

	Vector3 GetWireAnchorPosition() const override { return m_Position; }
	bool IsWireTargetActive() const override { return !m_Broken && !m_Destroy; }
	float GetWireTargetRadius() const override;
	GameObject* GetWireTargetObject() override { return this; }

	Vector3 GetBreakableCenter() const override { return m_Position; }
	float GetBreakableRadius() const override;
	bool IsBreakableAlive() const override { return !m_Broken && !m_Destroy; }
	Kind GetBreakableKind() const override { return Kind::Lock; }
	bool ApplyBreakDamage(int damage, const Vector3& hitDirection) override;
};
