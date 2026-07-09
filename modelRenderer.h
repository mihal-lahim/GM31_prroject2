#pragma once



// マテリアル構造体
struct MODEL_MATERIAL
{
	char						Name[256];
	MATERIAL					Material;
	char						TextureName[256];
	ComPtr<ID3D11ShaderResourceView>	Texture;

};


// 描画サブセット構造体
struct SUBSET
{
	unsigned int	StartIndex;
	unsigned int	IndexNum;
	MODEL_MATERIAL	Material;
};


// モデル構造体
struct MODEL_OBJ
{
	std::vector<VERTEX_3D> VertexArray;
	unsigned int	VertexNum;

	std::vector<unsigned int> IndexArray;
	unsigned int	IndexNum;

	std::vector<SUBSET> SubsetArray;
	unsigned int	SubsetNum;
};

struct MODEL
{
	ComPtr<ID3D11Buffer>	VertexBuffer;
	ComPtr<ID3D11Buffer>	IndexBuffer;

	std::vector<SUBSET> SubsetArray;
	unsigned int	SubsetNum;
};


#include "component.h"
#include <string>
#include <unordered_map>


class ModelRenderer : public Component
{
private:

	static std::unordered_map<std::string, std::unique_ptr<MODEL>> m_ModelPool;
	static ComPtr<ID3D11InputLayout> m_DefaultVertexLayout;
	static ComPtr<ID3D11VertexShader> m_DefaultVertexShader;
	static ComPtr<ID3D11PixelShader> m_DefaultPixelShader;

	static void LoadModel(const char *FileName, MODEL *Model);
	static void LoadObj( const char *FileName, MODEL_OBJ *ModelObj );
	static void LoadMaterial( const char *FileName, std::vector<MODEL_MATERIAL>& MaterialArray );
	static void EnsureDefaultShader();

	MODEL* m_Model{};
	bool m_UseOwnerTransform = false;
	Vector3 m_RotationOffset{ 0.0f, 0.0f, 0.0f };

public:

	static void Preload( const char *FileName );
	static void UnloadAll();


	using Component::Component;

	void Load( const char *FileName );
	ModelRenderer* SetUseOwnerTransform(bool enabled = true);
	ModelRenderer* SetRotationOffset(const Vector3& offset);
	void Draw() override;

};
