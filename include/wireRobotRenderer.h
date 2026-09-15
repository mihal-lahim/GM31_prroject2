#pragma once

#include "component.h"
#include "renderer.h"

// 配布モデルに依存しない、直方体だけで作る箱型ロボット描画です。
// プレイヤーと敵が工場設備と同じ造形言語になるよう、WireBoxMeshを共有します。
class WireRobotRenderer : public Component
{
public:
	enum class Style { Player, TroubleBot, Drone };

private:
	Style m_Style;
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	unsigned int m_IndexCount = 0;
	bool m_Available = false;
	float m_AnimationTime = 0.0f;
	float m_LandTimer = 0.0f;
	float m_LandImpact = 0.0f;
	float m_JumpTimer = 0.0f;
	float m_WireShootTimer = 0.0f;
	float m_StumbleTimer = 0.0f;
	bool m_JumpPoseActive = false;
	bool m_PreviousGrounded = false;
	bool m_PreviousWireActive = false;
	Vector3 m_PreviousVelocity{ 0.0f,0.0f,0.0f };
	float m_WireRoll = 0.0f;

public:
	WireRobotRenderer(GameObject* owner, Style style) : Component(owner), m_Style(style) {}
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
