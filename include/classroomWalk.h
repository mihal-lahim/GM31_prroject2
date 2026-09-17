#pragma once

#include "scene.h"
#include "gameObject.h"
#include "resourceManager.h"
#include "cameraView.h"
#include "dynamicText.h"
#include "wave.h"

#include <chrono>

class AnimationModel;

class ClassroomPlayer final : public GameObject
{
public:
	void Init() override;
	void Update() override;
	bool GetFootClearance(float (&clearance)[2]) const;
	bool GetFootEndClearance(float (&clearance)[2][2]) const;
private:
	AnimationModel* m_Model = nullptr;
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
	void Init() override;
	void Draw() override;
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	UINT m_IndexCount = 0;
	ComPtr<ID3D11InputLayout> m_Layout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	TextureHandle m_Texture;
};

class ClassroomTree final : public GameObject
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
