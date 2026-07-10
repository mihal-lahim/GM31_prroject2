#pragma once

#include "component.h"
#include "renderer.h"
#include "resourceManager.h"
#include <string>
#include <unordered_map>



// マテリアル構造体
struct MODEL_MATERIAL
{
	std::string					Name;
	MATERIAL					Material{};
	std::string					TextureName;
	TextureHandle Texture;

};


// 描画サブセット構造体
struct SUBSET
{
	unsigned int	StartIndex = 0;
	unsigned int	IndexNum = 0;
	MODEL_MATERIAL	Material;
};


// モデル構造体
struct MODEL_OBJ
{
	std::vector<VERTEX_3D> VertexArray;
	unsigned int	VertexNum = 0;

	std::vector<unsigned int> IndexArray;
	unsigned int	IndexNum = 0;

	std::vector<SUBSET> SubsetArray;
	unsigned int	SubsetNum = 0;
};

struct MODEL
{
	ComPtr<ID3D11Buffer>	VertexBuffer;
	ComPtr<ID3D11Buffer>	IndexBuffer;

	std::vector<SUBSET> SubsetArray;
	unsigned int	SubsetNum = 0;
};

class ModelRenderer : public Component
{
private:

	static ComPtr<ID3D11InputLayout> m_DefaultVertexLayout;
	static ComPtr<ID3D11VertexShader> m_DefaultVertexShader;
	static ComPtr<ID3D11PixelShader> m_DefaultPixelShader;

	static bool LoadModel(const char *FileName, MODEL *Model);
	static void EnsureDefaultShader();
	static ModelHandle AcquireModel(const char* FileName);

	ModelHandle m_Model;
	bool m_UseOwnerTransform = false;
	Vector3 m_RotationOffset{ 0.0f, 0.0f, 0.0f };

public:

	static void Preload( const char *FileName );
	static void ReleaseSharedResources();


	using Component::Component;

	void Load( const char *FileName );
	ModelRenderer* SetUseOwnerTransform(bool enabled = true);
	ModelRenderer* SetRotationOffset(const Vector3& offset);
	void Draw() override;

};
