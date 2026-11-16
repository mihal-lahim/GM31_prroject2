#pragma once

#include "gameObject.h"
#include "resourceManager.h"
#include "renderer.h"
#include "surfaceMaterial.h"

#include <vector>

namespace ClassroomLake
{
	inline constexpr float CenterX = 0.0f;
	inline constexpr float CenterZ = 11.0f;
	inline constexpr float WaterLevel = 0.20f;
	inline constexpr float WaterRadius = 9.0f;
	inline constexpr float FloorRadius = 7.0f;
	inline constexpr float ShoreRadius = 9.5f;
	inline constexpr float BasinRadius = 12.0f;
}

class ClassroomWater final : public GameObject
{
public:
	void Init() override;
	void Draw() override;
	static float GetSurfaceHeight(float x, float z, float time);

private:
	static constexpr int kRingCount = 12;
	static constexpr int kSegmentCount = 32;
	static constexpr float kWaveAmplitude = 0.12f;
	static constexpr float kWaveLength = 3.2f;
	static constexpr float kWaveCycle = 2.6f;

	std::vector<Engine::VERTEX_3D> m_Vertices;
	std::vector<unsigned int> m_Indices;
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	ComPtr<ID3D11InputLayout> m_Layout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	Engine::TextureHandle m_Texture;
	Engine::SurfaceMaterial m_Surface;
	UINT m_IndexCount = 0;
	float m_BaseHeight = 0.0f;

	void BuildMesh();
	void AnimateMesh(float time);
	static float WaveHeight(float x, float z, float time);
};
