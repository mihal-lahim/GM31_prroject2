#include "main.h"
#include "wireFactory.h"

#include "graphicsResourceFactory.h"
#include "renderer.h"
#include "wireBoxMesh.h"

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
	//   建築 = クリーム / 搬送 = 水色 / 加工 = 黄色 / 動力 = オレンジ / 管理 = 黄緑
	const XMFLOAT4 kFloorColor{ 0.72f, 0.78f, 0.82f, 1.0f };
	const XMFLOAT4 kWallColor{ 0.91f, 0.89f, 0.80f, 1.0f };
	const XMFLOAT4 kStructureColor{ 1.00f, 0.72f, 0.18f, 1.0f };
	const XMFLOAT4 kKickWallColor{ 0.22f, 0.78f, 0.94f, 1.0f };
	const XMFLOAT4 kBeamColor{ 0.52f, 0.88f, 0.30f, 1.0f };
	const XMFLOAT4 kCreamColor{ 0.98f, 0.95f, 0.84f, 1.0f };
	const XMFLOAT4 kConveyorColor{ 0.18f, 0.72f, 0.92f, 1.0f };
	const XMFLOAT4 kPowerColor{ 1.00f, 0.42f, 0.18f, 1.0f };
	const XMFLOAT4 kPinkColor{ 1.00f, 0.42f, 0.68f, 1.0f };
}

void WireFactory::AddBox(
	std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
	const Vector3& center, const Vector3& halfSize, const XMFLOAT4& color)
{
	// 巻き順を含む生成処理は共通ヘルパーに集約しています。
	WireBoxMesh::Append(vertices, indices, center, halfSize, color);
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
	auto addDecoration = [&](const Vector3& center, const Vector3& halfSize, const XMFLOAT4& color)
	{
		AddBox(vertices, indices, center, halfSize, color);
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

	// --- 巨大なおもちゃ工場らしさを作る非衝突シルエット ---
	// 搬送ライン。水色の長いベルトとクリーム色の支持脚を繰り返し、奥行きを強調します。
	for (int i = 0; i < 6; ++i)
	{
		const float z = -68.0f + i * 25.0f;
		addDecoration({ -43.0f, 7.0f, z }, { 14.0f, 0.8f, 3.2f }, kConveyorColor);
		addDecoration({ -52.0f, 3.2f, z }, { 1.0f, 3.2f, 1.0f }, kCreamColor);
		addDecoration({ -34.0f, 3.2f, z }, { 1.0f, 3.2f, 1.0f }, kCreamColor);
		// 箱詰め製品はピンク・黄緑を交互にし、同じ機械の繰り返しをポップに見せます。
		addDecoration({ -47.0f + (i % 3) * 4.0f, 9.0f, z }, { 1.5f, 1.2f, 1.5f },
			i % 2 == 0 ? kPinkColor : kBeamColor);
	}
	// 右壁沿いの角型ダクトと大型ピストン。曲面を使わず、巨大さは太い断面で表現します。
	addDecoration({ 49.0f, 31.0f, 0.0f }, { 3.0f, 3.0f, 78.0f }, kCreamColor);
	for (int i = 0; i < 5; ++i)
	{
		const float z = -55.0f + i * 28.0f;
		addDecoration({ 42.0f, 24.0f, z }, { 6.0f, 2.4f, 4.0f }, kPowerColor);
		addDecoration({ 42.0f, 16.0f, z }, { 2.2f, 8.0f, 2.2f }, kCreamColor);
	}
	// 天井クレーン。遠景でも読める大きなコの字と機能色の吊りブロックです。
	addDecoration({ 0.0f, 43.0f, 18.0f }, { 48.0f, 1.8f, 2.8f }, kPinkColor);
	addDecoration({ 12.0f, 36.0f, 18.0f }, { 2.2f, 6.0f, 2.2f }, kCreamColor);
	addDecoration({ 12.0f, 29.0f, 18.0f }, { 4.0f, 2.0f, 4.0f }, kPowerColor);

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
