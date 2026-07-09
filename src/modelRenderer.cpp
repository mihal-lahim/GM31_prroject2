
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include "main.h"
#include "renderer.h"
#include "gameObject.h"
#include "modelRenderer.h"



std::unordered_map<std::string, std::unique_ptr<MODEL>> ModelRenderer::m_ModelPool;
ComPtr<ID3D11InputLayout> ModelRenderer::m_DefaultVertexLayout = nullptr;
ComPtr<ID3D11VertexShader> ModelRenderer::m_DefaultVertexShader = nullptr;
ComPtr<ID3D11PixelShader> ModelRenderer::m_DefaultPixelShader = nullptr;


void ModelRenderer::EnsureDefaultShader()
{
	if (m_DefaultVertexShader == nullptr || m_DefaultPixelShader == nullptr)
	{
		Renderer::CreateVertexShader(m_DefaultVertexShader, m_DefaultVertexLayout, "unlitTextureVS.cso");
		Renderer::CreatePixelShader(m_DefaultPixelShader, "unlitTexturePS.cso");
	}
}

void ModelRenderer::Draw()
{

	// 頂点バッファ設定
	if (m_Model == nullptr)
	{
		return;
	}

	EnsureDefaultShader();
	Renderer::GetDeviceContext()->IASetInputLayout(m_DefaultVertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_DefaultVertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_DefaultPixelShader.Get(), NULL, 0);

	if (m_UseOwnerTransform && m_GameObject != nullptr)
	{
		Vector3 scaleValue = m_GameObject->GetScale();
		Vector3 rotationValue = m_GameObject->GetRotation() + m_RotationOffset;
		Vector3 positionValue = m_GameObject->GetPosition();

		XMMATRIX scale = XMMatrixScaling(scaleValue.x, scaleValue.y, scaleValue.z);
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(rotationValue.x, rotationValue.y, rotationValue.z);
		XMMATRIX trans = XMMatrixTranslation(positionValue.x, positionValue.y, positionValue.z);
		Renderer::SetWorldMatrix(scale * rot * trans);
	}

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_Model->VertexBuffer.GetAddressOf(), &stride, &offset);

	// インデックスバッファ設定
	Renderer::GetDeviceContext()->IASetIndexBuffer(m_Model->IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// プリミティブトポロジ設定
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	for( unsigned int i = 0; i < m_Model->SubsetNum; i++ )
	{
		// マテリアル設定
		Renderer::SetMaterial(m_Model->SubsetArray[i].Material.Material );

		// テクスチャ設定
		if(m_Model->SubsetArray[i].Material.Texture)
			Renderer::GetDeviceContext()->PSSetShaderResources( 0, 1, m_Model->SubsetArray[i].Material.Texture.GetAddressOf() );

		// ポリゴン描画
		Renderer::GetDeviceContext()->DrawIndexed(m_Model->SubsetArray[i].IndexNum, m_Model->SubsetArray[i].StartIndex, 0 );
	}

}

void ModelRenderer::Preload(const char *FileName)
{
	if (m_ModelPool.count(FileName) > 0)
	{
		return;
	}

	auto model = std::make_unique<MODEL>();
	LoadModel(FileName, model.get());

	m_ModelPool[FileName] = std::move(model);

}


void ModelRenderer::UnloadAll()
{
	m_ModelPool.clear();
	m_DefaultVertexLayout.Reset();
	m_DefaultVertexShader.Reset();
	m_DefaultPixelShader.Reset();
}


void ModelRenderer::Load(const char *FileName)
{
	if (m_ModelPool.count(FileName) > 0)
	{
		m_Model = m_ModelPool[FileName].get();
		return;
	}

	auto model = std::make_unique<MODEL>();
	m_Model = model.get();
	LoadModel(FileName, m_Model);

	m_ModelPool[FileName] = std::move(model);

}

ModelRenderer* ModelRenderer::SetUseOwnerTransform(bool enabled)
{
	m_UseOwnerTransform = enabled;
	return this;
}

ModelRenderer* ModelRenderer::SetRotationOffset(const Vector3& offset)
{
	m_RotationOffset = offset;
	return this;
}

void ModelRenderer::LoadModel( const char *FileName, MODEL *Model)
{

	MODEL_OBJ modelObj;
	LoadObj( FileName, &modelObj );



	// 頂点バッファ生成
	{
		D3D11_BUFFER_DESC bd;
		ZeroMemory( &bd, sizeof(bd) );
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof( VERTEX_3D ) * modelObj.VertexNum;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd;
		ZeroMemory( &sd, sizeof(sd) );
		sd.pSysMem = modelObj.VertexArray.data();

		Renderer::GetDevice()->CreateBuffer( &bd, &sd, Model->VertexBuffer.GetAddressOf() );
	}


	// インデックスバッファ生成
	{
		D3D11_BUFFER_DESC bd;
		ZeroMemory( &bd, sizeof(bd) );
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof( unsigned int ) * modelObj.IndexNum;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd;
		ZeroMemory( &sd, sizeof(sd) );
		sd.pSysMem = modelObj.IndexArray.data();

		Renderer::GetDevice()->CreateBuffer( &bd, &sd, Model->IndexBuffer.GetAddressOf() );
	}

	// サブセット設定
	{
		Model->SubsetArray.resize(modelObj.SubsetNum);
		Model->SubsetNum = modelObj.SubsetNum;

		for( unsigned int i = 0; i < modelObj.SubsetNum; i++ )
		{
			Model->SubsetArray[i].StartIndex = modelObj.SubsetArray[i].StartIndex;
			Model->SubsetArray[i].IndexNum = modelObj.SubsetArray[i].IndexNum;

			Model->SubsetArray[i].Material.Material = modelObj.SubsetArray[i].Material.Material;

			Model->SubsetArray[i].Material.Texture.Reset();

			// テクスチャ読み込み
			TexMetadata metadata;
			ScratchImage image;
			wchar_t wc[256];
			mbstowcs(wc, modelObj.SubsetArray[i].Material.TextureName, sizeof(wc));
			LoadFromWICFile(wc, WIC_FLAGS_NONE, &metadata, image);
			CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, Model->SubsetArray[i].Material.Texture.GetAddressOf());

			if (Model->SubsetArray[i].Material.Texture)
				Model->SubsetArray[i].Material.Material.TextureEnable = true;
			else
				Model->SubsetArray[i].Material.Material.TextureEnable = false;

		}
	}

}






//モデル読込////////////////////////////////////////////
void ModelRenderer::LoadObj( const char *FileName, MODEL_OBJ *ModelObj )
{

	char dir[MAX_PATH];
	strcpy (dir, FileName );
	PathRemoveFileSpec(dir);





	std::vector<XMFLOAT3> positionArray;
	std::vector<XMFLOAT3> normalArray;
	std::vector<XMFLOAT2> texcoordArray;

	unsigned int	positionNum = 0;
	unsigned int	normalNum = 0;
	unsigned int	texcoordNum = 0;
	unsigned int	vertexNum = 0;
	unsigned int	indexNum = 0;
	unsigned int	in = 0;
	unsigned int	subsetNum = 0;

	std::vector<MODEL_MATERIAL> materialArray;

	char str[256];
	char *s;
	int c;


	FILE *file;
	file = fopen( FileName, "rt" );
	assert(file);



	//要素数カウント
	while( true )
	{
		fscanf( file, "%s", str );

		if( feof( file ) != 0 )
			break;

		if( strcmp( str, "v" ) == 0 )
		{
			positionNum++;
		}
		else if( strcmp( str, "vn" ) == 0 )
		{
			normalNum++;
		}
		else if( strcmp( str, "vt" ) == 0 )
		{
			texcoordNum++;
		}
		else if( strcmp( str, "usemtl" ) == 0 )
		{
			subsetNum++;
		}
		else if( strcmp( str, "f" ) == 0 )
		{
			in = 0;

			do
			{
				fscanf( file, "%s", str );
				vertexNum++;
				in++;
				c = fgetc( file );
			}
			while( c != '\n' && c!= '\r' );

			//四角は三角に分割
			if( in == 4 )
				in = 6;

			indexNum += in;
		}
	}


	//メモリ確保
	positionArray.resize(positionNum);
	normalArray.resize(normalNum);
	texcoordArray.resize(texcoordNum);


	ModelObj->VertexArray.resize(vertexNum);
	ModelObj->VertexNum = vertexNum;

	ModelObj->IndexArray.resize(indexNum);
	ModelObj->IndexNum = indexNum;

	ModelObj->SubsetArray.resize(subsetNum);
	ModelObj->SubsetNum = subsetNum;




	//要素読込
	XMFLOAT3 *position = positionArray.data();
	XMFLOAT3 *normal = normalArray.data();
	XMFLOAT2 *texcoord = texcoordArray.data();

	unsigned int vc = 0;
	unsigned int ic = 0;
	unsigned int sc = 0;


	fseek( file, 0, SEEK_SET );

	while( true )
	{
		fscanf( file, "%s", str );

		if( feof( file ) != 0 )
			break;

		if( strcmp( str, "mtllib" ) == 0 )
		{
			//マテリアルファイル
			fscanf( file, "%s", str );

			char path[256];
			strcpy( path, dir );
			strcat( path, "\\" );
			strcat( path, str );

			LoadMaterial( path, materialArray );
		}
		else if( strcmp( str, "o" ) == 0 )
		{
			//オブジェクト名
			fscanf( file, "%s", str );
		}
		else if( strcmp( str, "v" ) == 0 )
		{
			//頂点座標
			fscanf( file, "%f", &position->x );
			fscanf( file, "%f", &position->y );
			fscanf( file, "%f", &position->z );
			position++;
		}
		else if( strcmp( str, "vn" ) == 0 )
		{
			//法線
			fscanf( file, "%f", &normal->x );
			fscanf( file, "%f", &normal->y );
			fscanf( file, "%f", &normal->z );
			normal++;
		}
		else if( strcmp( str, "vt" ) == 0 )
		{
			//テクスチャ座標
			fscanf( file, "%f", &texcoord->x );
			fscanf( file, "%f", &texcoord->y );
			texcoord->x = 1.0f - texcoord->x;
			texcoord->y = 1.0f - texcoord->y;
			texcoord++;
		}
		else if( strcmp( str, "usemtl" ) == 0 )
		{
			//マテリアル
			fscanf( file, "%s", str );

			if( sc != 0 )
				ModelObj->SubsetArray[ sc - 1 ].IndexNum = ic - ModelObj->SubsetArray[ sc - 1 ].StartIndex;

			ModelObj->SubsetArray[ sc ].StartIndex = ic;


			for( unsigned int i = 0; i < materialArray.size(); i++ )
			{
				if( strcmp( str, materialArray[i].Name ) == 0 )
				{
					ModelObj->SubsetArray[ sc ].Material.Material = materialArray[i].Material;
					strcpy( ModelObj->SubsetArray[ sc ].Material.TextureName, materialArray[i].TextureName );
					strcpy( ModelObj->SubsetArray[ sc ].Material.Name, materialArray[i].Name );

					break;
				}
			}

			sc++;
			
		}
		else if( strcmp( str, "f" ) == 0 )
		{
			//面
			in = 0;

			do
			{
				fscanf( file, "%s", str );

				s = strtok( str, "/" );	
				ModelObj->VertexArray[vc].Position = positionArray[ atoi( s ) - 1 ];
				if( s[ strlen( s ) + 1 ] != '/' )
				{
					//テクスチャ座標が存在しない場合もある
					s = strtok( nullptr, "/" );
					ModelObj->VertexArray[vc].TexCoord = texcoordArray[ atoi( s ) - 1 ];
				}
				s = strtok( nullptr, "/" );	
				ModelObj->VertexArray[vc].Normal = normalArray[ atoi( s ) - 1 ];

				ModelObj->VertexArray[vc].Diffuse = XMFLOAT4( 1.0f, 1.0f, 1.0f, 1.0f );

				ModelObj->IndexArray[ic] = vc;
				ic++;
				vc++;

				in++;
				c = fgetc( file );
			}
			while( c != '\n' && c != '\r' );

			//四角は三角に分割
			if( in == 4 )
			{
				ModelObj->IndexArray[ic] = vc - 4;
				ic++;
				ModelObj->IndexArray[ic] = vc - 2;
				ic++;
			}
		}
	}


	if( sc != 0 )
		ModelObj->SubsetArray[ sc - 1 ].IndexNum = ic - ModelObj->SubsetArray[ sc - 1 ].StartIndex;


	fclose( file );


}




//マテリアル読み込み///////////////////////////////////////////////////////////////////
void ModelRenderer::LoadMaterial( const char *FileName, std::vector<MODEL_MATERIAL>& MaterialArray )
{

	char dir[MAX_PATH];
	strcpy(dir, FileName);
	PathRemoveFileSpec(dir);



	char str[256];

	FILE *file;
	file = fopen( FileName, "rt" );
	assert(file);

	unsigned int materialNum = 0;

	//要素数カウント
	while( true )
	{
		fscanf( file, "%s", str );

		if( feof( file ) != 0 )
			break;


		if( strcmp( str, "newmtl" ) == 0 )
		{
			materialNum++;
		}
	}


	//メモリ確保
	MaterialArray.resize(materialNum);


	//要素読込
	int mc = -1;

	fseek( file, 0, SEEK_SET );

	while( true )
	{
		fscanf( file, "%s", str );

		if( feof( file ) != 0 )
			break;


		if( strcmp( str, "newmtl" ) == 0 )
		{
			//マテリアル名
			mc++;
			fscanf( file, "%s", MaterialArray[ mc ].Name );
			strcpy( MaterialArray[ mc ].TextureName, "" );

			MaterialArray[mc].Material.Emission.x = 0.0f;
			MaterialArray[mc].Material.Emission.y = 0.0f;
			MaterialArray[mc].Material.Emission.z = 0.0f;
			MaterialArray[mc].Material.Emission.w = 0.0f;
		}
		else if( strcmp( str, "Ka" ) == 0 )
		{
			//アンビエント
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Ambient.x );
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Ambient.y );
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Ambient.z );
			MaterialArray[ mc ].Material.Ambient.w = 1.0f;
		}
		else if( strcmp( str, "Kd" ) == 0 )
		{
			//ディフューズ
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Diffuse.x );
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Diffuse.y );
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Diffuse.z );
			MaterialArray[ mc ].Material.Diffuse.w = 1.0f;
		}
		else if( strcmp( str, "Ks" ) == 0 )
		{
			//スペキュラ
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Specular.x );
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Specular.y );
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Specular.z );
			MaterialArray[ mc ].Material.Specular.w = 1.0f;
		}
		else if( strcmp( str, "Ns" ) == 0 )
		{
			//スペキュラ強度
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Shininess );
		}
		else if( strcmp( str, "d" ) == 0 )
		{
			//アルファ
			fscanf( file, "%f", &MaterialArray[ mc ].Material.Diffuse.w );
		}
		else if( strcmp( str, "map_Kd" ) == 0 )
		{
			//テクスチャ
			fscanf( file, "%s", str );

			char path[256];
			strcpy( path, dir );
			strcat( path, "\\" );
			strcat( path, str );

			strcat( MaterialArray[ mc ].TextureName, path );
		}
	}

	fclose(file);

}

