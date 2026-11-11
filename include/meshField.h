#pragma once

#include "gameObject.h"
#include "resourceManager.h"
#include "surfaceMaterial.h"

#include <string_view>

// 凹凸付きのメッシュフィールド(地形)。高さ関数から頂点グリッドを生成し、
// 描画と同じ三角形で足元の高さ・法線を返します。
class MeshField final : public GameObject
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
