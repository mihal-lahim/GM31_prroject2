#pragma once

#include "gameObject.h"
#include "wireBreakable.h"
#include "wireTarget.h"

class WireGiantMachine;

// 巨大機械を構成する部位です。
//
// 巨大機械を「HPの多い敵」ではなく「動くステージ」として扱うため、
// 部位ごとに役割(ワイヤー可能・破壊可能・足場・弱点)を設定できるようにしています。
// 装甲を壊す → 内部が露出する → 弱点が現れる、という流れは
// 部位同士の有効・無効の切り替えだけで表現します。
class WireMachinePart : public GameObject, public IWireTarget, public IWireBreakable
{
public:
	enum class Role
	{
		Armor,      // 装甲。壊すと内部が露出する
		WeakPoint,  // 弱点。装甲が壊れるまで露出しない
		Platform,   // 足場。乗って移動できる
		Structure   // 構造。掛かり所にはなるが壊せない
	};

private:
	Role m_Role = Role::Structure;
	WireGiantMachine* m_Owner = nullptr;

	// 親からの相対位置です。巨大機械が動くと部位も一緒に動きます。
	Vector3 m_LocalOffset{ 0.0f, 0.0f, 0.0f };
	Vector3 m_HalfSize{ 2.0f, 2.0f, 2.0f };

	int m_Health = 1;
	int m_MaxHealth = 1;
	bool m_Broken = false;
	// 装甲が壊れるまでは弱点を露出させないためのフラグです。
	bool m_Exposed = true;
	bool m_WireEnabled = true;
	float m_HitFlash = 0.0f;

	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	unsigned int m_IndexCount = 0;

	void BuildMesh();
	void EmitBreakEffect() const;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	// 生成直後に一度だけ呼び、役割と形状を確定させます。
	void Setup(WireGiantMachine* owner, Role role, const Vector3& localOffset,
		const Vector3& halfSize, int health);

	Role GetRole() const { return m_Role; }
	bool IsBroken() const { return m_Broken; }
	Vector3 GetHalfSize() const { return m_HalfSize; }
	Vector3 GetLocalOffset() const { return m_LocalOffset; }
	float GetHealthRatio01() const;

	// 装甲が壊れたときに、隠れていた弱点を露出させます。
	void SetExposed(bool exposed);
	bool IsExposed() const { return m_Exposed; }

	// 部位の天面に乗れるかどうか。足場として使う部位だけtrueです。
	bool IsLandable() const { return m_Role == Role::Platform || m_Role == Role::Armor; }
	float GetTopY() const { return m_Position.y + m_HalfSize.y; }
	bool ContainsXZ(const Vector3& position, float margin) const;

	Vector3 GetWireAnchorPosition() const override;
	bool IsWireTargetActive() const override;
	float GetWireTargetRadius() const override;
	GameObject* GetWireTargetObject() override { return this; }

	Vector3 GetBreakableCenter() const override { return m_Position; }
	float GetBreakableRadius() const override;
	bool IsBreakableAlive() const override;
	Kind GetBreakableKind() const override;
	bool ApplyBreakDamage(int damage, const Vector3& hitDirection) override;
};
