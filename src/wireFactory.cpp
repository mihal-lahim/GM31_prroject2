#include "main.h"
#include "wireFactory.h"

#include "graphicsResourceFactory.h"
#include "renderer.h"

#include <cmath>

namespace
{
	// テストステージの寸法です。「侵入 → 高所へ → 進む → 敵 → 巨大機械」を
	// 一本の動線で確認できる最小限の広さに絞っています。
	constexpr float kFloorHalfX = 62.0f;
	constexpr float kFloorHalfZ = 96.0f;
	constexpr float kWallHeight = 46.0f;
	constexpr float kWallThickness = 3.0f;

	// 色で役割を伝えます。全部が同じ灰色だと、どこに乗れてどこを蹴れるのかが
	// 見て分からず、ゲーム内容がぱっと読めなくなるためです。
	//   床・壁 = 暗い工業色 / 足場 = 明るい黄土 / 蹴れる壁 = 青 / 梁 = シアン
	const XMFLOAT4 kFloorColor{ 0.13f, 0.16f, 0.21f, 1.0f };
	const XMFLOAT4 kWallColor{ 0.17f, 0.20f, 0.26f, 1.0f };
	const XMFLOAT4 kStructureColor{ 0.62f, 0.52f, 0.28f, 1.0f };
	const XMFLOAT4 kKickWallColor{ 0.20f, 0.44f, 0.72f, 1.0f };
	const XMFLOAT4 kBeamColor{ 0.22f, 0.62f, 0.72f, 1.0f };
}

void WireFactory::AddBox(
	std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
	const Vector3& center, const Vector3& halfSize, const XMFLOAT4& color)
{
	const unsigned int base = static_cast<unsigned int>(vertices.size());

	// 面ごとに法線と明るさを変え、テクスチャ無しでも立体が読めるようにします。
	struct Face
	{
		Vector3 Normal;
		Vector3 Right;
		Vector3 Up;
		float Shade;
	};
	const Face faces[6] = {
		{ {  0,  1,  0 }, { 1, 0, 0 }, { 0, 0, 1 }, 1.00f },
		{ {  0, -1,  0 }, { 1, 0, 0 }, { 0, 0,-1 }, 0.45f },
		{ {  1,  0,  0 }, { 0, 0,-1 }, { 0, 1, 0 }, 0.78f },
		{ { -1,  0,  0 }, { 0, 0, 1 }, { 0, 1, 0 }, 0.70f },
		{ {  0,  0,  1 }, { 1, 0, 0 }, { 0, 1, 0 }, 0.86f },
		{ {  0,  0, -1 }, {-1, 0, 0 }, { 0, 1, 0 }, 0.62f },
	};

	for (const Face& face : faces)
	{
		const Vector3 faceCenter{
			center.x + face.Normal.x * halfSize.x,
			center.y + face.Normal.y * halfSize.y,
			center.z + face.Normal.z * halfSize.z };
		const Vector3 right{
			face.Right.x * halfSize.x, face.Right.y * halfSize.y, face.Right.z * halfSize.z };
		const Vector3 up{
			face.Up.x * halfSize.x, face.Up.y * halfSize.y, face.Up.z * halfSize.z };
		const XMFLOAT4 shaded{
			color.x * face.Shade, color.y * face.Shade, color.z * face.Shade, color.w };

		const unsigned int faceBase = static_cast<unsigned int>(vertices.size());
		const Vector3 corners[4] = {
			faceCenter - right - up,
			faceCenter + right - up,
			faceCenter + right + up,
			faceCenter - right + up };
		const XMFLOAT2 uvs[4] = { {0,1}, {1,1}, {1,0}, {0,0} };
		for (int i = 0; i < 4; ++i)
		{
			VERTEX_3D vertex{};
			vertex.Position = { corners[i].x, corners[i].y, corners[i].z };
			vertex.Normal = { face.Normal.x, face.Normal.y, face.Normal.z };
			vertex.Diffuse = shaded;
			vertex.TexCoord = uvs[i];
			vertices.push_back(vertex);
		}
		indices.push_back(faceBase + 0);
		indices.push_back(faceBase + 2);
		indices.push_back(faceBase + 1);
		indices.push_back(faceBase + 0);
		indices.push_back(faceBase + 3);
		indices.push_back(faceBase + 2);
	}
	(void)base;
}

void WireFactory::BuildGeometry()
{
	std::vector<VERTEX_3D> vertices;
	std::vector<unsigned int> indices;
	m_BlockField.Clear();
	m_HighPerchPoints.clear();

	auto addSolid = [&](const Vector3& center, const Vector3& halfSize,
		const XMFLOAT4& color, bool landable = true)
	{
		AddBox(vertices, indices, center, halfSize, color);
		m_BlockField.Add(center, halfSize, landable);
	};

	// --- 床 ---
	addSolid({ 0.0f, -1.5f, 0.0f }, { kFloorHalfX, 1.5f, kFloorHalfZ }, kFloorColor);

	// --- 外壁。壁蹴りの足場にもなるため、内側を垂直に保ちます ---
	addSolid({ -kFloorHalfX, kWallHeight * 0.5f, 0.0f },
		{ kWallThickness, kWallHeight * 0.5f, kFloorHalfZ }, kWallColor, false);
	addSolid({ kFloorHalfX, kWallHeight * 0.5f, 0.0f },
		{ kWallThickness, kWallHeight * 0.5f, kFloorHalfZ }, kWallColor, false);
	addSolid({ 0.0f, kWallHeight * 0.5f, -kFloorHalfZ },
		{ kFloorHalfX, kWallHeight * 0.5f, kWallThickness }, kWallColor, false);
	addSolid({ 0.0f, kWallHeight * 0.5f, kFloorHalfZ },
		{ kFloorHalfX, kWallHeight * 0.5f, kWallThickness }, kWallColor, false);

	// --- 侵入エリア。低い足場から徐々に高くなり、自然に上を目指させます ---
	// 足場は動線の左右へ寄せています。正面に置くと開始直後の視界を塞ぎ、
	// 奥の巨大機械という目標が見えなくなるためです。
	addSolid({ 0.0f, 1.6f, -78.0f }, { 12.0f, 1.6f, 8.0f }, kStructureColor);
	addSolid({ -30.0f, 4.5f, -62.0f }, { 7.0f, 4.5f, 7.0f }, kStructureColor);
	addSolid({ 30.0f, 8.0f, -50.0f }, { 7.0f, 8.0f, 7.0f }, kStructureColor);

	// --- 壁蹴りで登る隘路。左右の壁が近く、蹴って登れる幅にしてあります ---
	addSolid({ -22.0f, 20.0f, -30.0f }, { 5.0f, 20.0f, 9.0f }, kKickWallColor, false);
	addSolid({ 22.0f, 20.0f, -30.0f }, { 5.0f, 20.0f, 9.0f }, kKickWallColor, false);
	addSolid({ 0.0f, 21.0f, -22.0f }, { 9.0f, 1.2f, 6.0f }, kStructureColor);
	m_HighPerchPoints.push_back({ 0.0f, 22.2f, -22.0f });

	// --- 中央の作業区画。小型ロボットとの交戦場所です ---
	addSolid({ -26.0f, 13.0f, 0.0f }, { 10.0f, 1.4f, 12.0f }, kStructureColor);
	addSolid({ 26.0f, 17.0f, 6.0f }, { 10.0f, 1.4f, 12.0f }, kStructureColor);
	addSolid({ 0.0f, 24.0f, 12.0f }, { 11.0f, 1.4f, 9.0f }, kStructureColor);
	m_HighPerchPoints.push_back({ -26.0f, 14.4f, 0.0f });
	m_HighPerchPoints.push_back({ 26.0f, 18.4f, 6.0f });
	m_HighPerchPoints.push_back({ 0.0f, 25.4f, 12.0f });

	// --- 巨大機械が立つ奥のホール。周囲に飛び回るための足場を配置します ---
	addSolid({ -34.0f, 20.0f, 52.0f }, { 8.0f, 1.4f, 10.0f }, kStructureColor);
	addSolid({ 34.0f, 26.0f, 52.0f }, { 8.0f, 1.4f, 10.0f }, kStructureColor);
	addSolid({ 0.0f, 32.0f, 74.0f }, { 12.0f, 1.4f, 8.0f }, kStructureColor);
	m_HighPerchPoints.push_back({ -34.0f, 21.4f, 52.0f });
	m_HighPerchPoints.push_back({ 34.0f, 27.4f, 52.0f });
	m_HighPerchPoints.push_back({ 0.0f, 33.4f, 74.0f });

	// --- 天井の梁。ワイヤーで一気に高所へ行くための的です ---
	for (int i = 0; i < 5; ++i)
	{
		const float z = -60.0f + i * 32.0f;
		addSolid({ 0.0f, 40.0f, z }, { kFloorHalfX - 4.0f, 1.2f, 2.4f }, kBeamColor, false);
	}

	// 侵入台の手前寄りに立たせます。壁際すぎるとカメラが背後の壁に押し込まれ、
	// 開始した瞬間に工場の全体像が見えなくなるためです。
	m_SpawnPoint = { 0.0f, 4.0f, -74.0f };

	D3D11_BUFFER_DESC vertexDesc{};
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(VERTEX_3D));
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices.data();

	D3D11_BUFFER_DESC indexDesc{};
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(unsigned int));
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();

	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, vertexDesc, &vertexData, "WireFactoryVertices");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, indexDesc, &indexData, "WireFactoryIndices") && m_RenderingAvailable;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireFactory") && m_RenderingAvailable;
	m_IndexCount = static_cast<unsigned int>(indices.size());
}

void WireFactory::Init()
{
	m_Layer = Layer::World;
	BuildGeometry();
}

void WireFactory::Uninit()
{
	GameObject::Uninit();
}

void WireFactory::Draw()
{
	if (m_IndexCount == 0)
	{
		return;
	}

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(XMMatrixIdentity());

	MATERIAL material{};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.Ambient = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::GetDeviceContext()->DrawIndexed(m_IndexCount, 0, 0);

	GameObject::Draw();
}
