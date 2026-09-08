#pragma once

#include "gameObject.h"
#include "resourceManager.h"
#include "wireTarget.h"

// ワイヤーを掛けられる地点を示すビルボードです。
//
// 配達マーカーとして使われていたものを、本来のワイヤー可能地点表示へ戻し、
// IWireTargetを実装させました。工場設備(クレーン・搬送機・配管)の
// 掛かり所はこのオブジェクトを置くことで表現します。
class WireAnchor : public GameObject, public IWireTarget
{
private:
	static ComPtr<ID3D11Buffer> s_VertexBuffer;
	static ComPtr<ID3D11InputLayout> s_VertexLayout;
	static ComPtr<ID3D11VertexShader> s_VertexShader;
	static ComPtr<ID3D11PixelShader> s_PixelShader;
	static TextureHandle s_Texture;
	static int s_RefCount;

	float m_Radius = 1.4f;
	float m_Pulse = 0.0f;
	bool m_Visible = true;
	bool m_Active = true;
	// 狙われている間だけ強く光らせ、掛けられることを一目で分かるようにします。
	float m_Highlight = 0.0f;
	XMFLOAT4 m_OuterColor{ 0.08f, 0.72f, 1.0f, 0.72f };
	XMFLOAT4 m_InnerColor{ 1.0f, 1.0f, 1.0f, 0.95f };

public:
	explicit WireAnchor(bool visible = true) : m_Visible(visible) {}
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	Vector3 GetHookPosition() const { return m_Position; }
	float GetRadius() const { return m_Radius; }
	void SetRadius(float radius) { m_Radius = radius; }
	void SetColors(const XMFLOAT4& outer, const XMFLOAT4& inner) { m_OuterColor = outer; m_InnerColor = inner; }
	void SetWireTargetActive(bool active) { m_Active = active; }
	// 現在狙われている対象として強調表示します。毎フレーム狙い側から設定します。
	void SetHighlighted() { m_Highlight = 1.0f; }

	Vector3 GetWireAnchorPosition() const override { return m_Position; }
	bool IsWireTargetActive() const override { return m_Active && !m_Destroy; }
	float GetWireTargetRadius() const override { return m_Radius; }
	GameObject* GetWireTargetObject() override { return this; }
};
