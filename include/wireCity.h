#pragma once

#include "gameObject.h"

#include <vector>

class WireCity : public GameObject
{
private:
	struct BuildingBounds
	{
		float CenterX;
		float CenterZ;
		float HalfWidth;
		float HalfDepth;
		float Height;
	};

	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	unsigned int m_IndexCount = 0;
	std::vector<Vector3> m_DeliveryLocations;
	std::vector<BuildingBounds> m_BuildingBounds;

public:
	void Init() override;
	void Uninit() override;
	void Draw() override;
	void ResolvePlayerCollision(const Vector3& previousPosition, Vector3& position, Vector3& velocity) const;
	const std::vector<Vector3>& GetDeliveryLocations() const { return m_DeliveryLocations; }
};
