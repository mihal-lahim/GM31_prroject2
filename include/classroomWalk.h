#pragma once

#include "scene.h"
#include "gameObject.h"
#include "resourceManager.h"
#include "cameraView.h"
#include "dynamicText.h"
#include "wave.h"
#include "surfaceMaterial.h"

#include <chrono>

class AnimationModel;
class ModelRenderer;
namespace Engine { class Cloth; }

class ClassroomPlayer final : public GameObject
{
public:
	void Init() override;
	void Update() override;
	bool GetFootClearance(float (&clearance)[2]) const;
	bool GetFootEndClearance(float (&clearance)[2][2]) const;
	// 材質の差し替えはレンダーテストでの検証用です。UseDefaultShaderで写実表現へ戻します。
	bool SetShaderDefinition(std::string_view id);
	void UseDefaultShader();
	std::string_view GetAnimationName() const;
	bool GetSwimAnchor(Vector3& position) const;
private:
	AnimationModel* m_Model = nullptr;
	bool m_Dancing = false;
	float m_VerticalSpeed = 0.0f;
	float m_FootIKBlend = 1.0f;
	float m_WaterAlignment = 0.0f;
	float m_WaterModelOffset = 0.0f;
	void AlignFeetToField();
};

class ClassroomCamera final : public GameObject, public ICameraView
{
public:
	void Init() override;
	void PreUpdate() override;
	void LateUpdate() override;
	void Draw() override;
	float GetYaw() const { return m_Yaw; }
	XMMATRIX GetViewMatrix() const override { return m_View; }
	Vector3 GetCameraPosition() const override { return m_Position; }
	Vector3 GetCameraForward() const override;
private:
	XMMATRIX m_View = XMMatrixIdentity();
	Vector3 m_Target{};
	float m_Yaw = 0.0f;
	float m_Pitch = 0.0f;
};

class ClassroomField final : public GameObject
{
public:
	static float GetHeight(float x, float z);
	static Vector3 GetNormal(float x, float z);
	// 通常は写実表現(realistic)です。ほかのシェーダーへの差し替えはレンダーテストでの検証用です。
	bool UseRealisticShader();
	bool SetShaderMode(Engine::SurfaceShader shader);
	bool SetShaderDefinition(std::string_view id);
	void Init() override;
	void Draw() override;
	// 丘が自分の裏側や周りに影を落とします。
	void DrawShadow() override;
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	UINT m_IndexCount = 0;
	ComPtr<ID3D11InputLayout> m_Layout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	TextureHandle m_Texture;
	Engine::SurfaceMaterial m_Surface;
};

class ClassroomTree final : public GameObject
{
public:
	void Init() override;
	void Draw() override;
	// 見た目の板はカメラを向きますが、影は太陽へ向けた板で落とします(葉の形に抜いた影になります)。
	void DrawShadow() override;
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_Layout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	TextureHandle m_Texture;
};

class ClassroomGrass final : public GameObject
{
public:
	void Init() override;
	void Draw() override;
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_Layout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	TextureHandle m_Texture;
};

// 湖畔の入口に掛けたのれんです。柱と横木はこのオブジェクトが描き、布は Engine::Cloth に任せます。
// 布は周囲のColliderに押されて動くので、プレイヤーが歩いてくぐると体に沿ってめくれます。
class ClassroomNoren final : public GameObject
{
public:
	// スタート地点から W で少し進んだ先、水際の手前です。
	static constexpr float PositionX = 0.0f;
	static constexpr float PositionZ = 1.3f;

	void Init() override;
	void Draw() override;
	void DrawShadow() override;
	Engine::Cloth* GetCloth() const { return m_Cloth; }

private:
	Engine::Cloth* m_Cloth = nullptr;
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	UINT m_IndexCount = 0;
	Engine::SurfaceMaterial m_Surface;
	TextureHandle m_ClothTexture;

	void BuildFrame(float groundHeight);
	void CreateClothTexture();
};

class ClassroomFps final : public DynamicText
{
public:
	void Init() override;
	void Draw() override;
private:
	using Clock = std::chrono::steady_clock;
	Clock::time_point m_LastSample{};
	int m_Frames = 0;
};

class ClassroomWalkScene final : public Scene
{
public:
	void Init() override;
	void Uninit() override {}
	void Update() override {}
	void Draw() override {}
};
