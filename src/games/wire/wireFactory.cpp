#include "main.h"
#include "wireFactory.h"

#include "graphicsResourceFactory.h"
#include "renderer.h"
#include "wireBoxMesh.h"
#include "wireAnchor.h"
#include "wirePlayer.h"
#include "gameTime.h"
#include "world.h"

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
	std::vector<Engine::VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
	const Vector3& center, const Vector3& halfSize, const XMFLOAT4& color)
{
	// 巻き順を含む生成処理は共通ヘルパーに集約しています。
	WireBoxMesh::Append(vertices, indices, center, halfSize, color);
}

void WireFactory::BuildGeometry()
{
	std::vector<Engine::VERTEX_3D> vertices;
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

	if (m_Layout == Layout::Training)
	{
		// 練習場。教える操作ごとに1つずつ仕掛けを置いた、見渡せる広さの部屋です。
		constexpr float kHalfX = 30.0f;
		constexpr float kHalfZ = 34.0f;
		constexpr float kHeight = 30.0f;
		addSolid({ 0.0f, -1.5f, 0.0f }, { kHalfX, 1.5f, kHalfZ }, kFloorColor);
		addSolid({ -kHalfX, kHeight * 0.5f, 0.0f }, { kWallThickness, kHeight * 0.5f, kHalfZ }, kWallColor, false);
		addSolid({ kHalfX, kHeight * 0.5f, 0.0f }, { kWallThickness, kHeight * 0.5f, kHalfZ }, kWallColor, false);
		addSolid({ 0.0f, kHeight * 0.5f, -kHalfZ }, { kHalfX, kHeight * 0.5f, kWallThickness }, kWallColor, false);
		addSolid({ 0.0f, kHeight * 0.5f, kHalfZ }, { kHalfX, kHeight * 0.5f, kWallThickness }, kWallColor, false);

		// ワイヤー練習用の高い足場と、その上の梁
		addSolid({ 0.0f, 9.0f, 8.0f }, { 6.0f, 1.2f, 5.0f }, kStructureColor);
		addSolid({ 0.0f, 22.0f, 8.0f }, { 10.0f, 1.0f, 2.0f }, kBeamColor, false);

		// 壁蹴り練習用の向かい合う2枚の壁。間隔6mで、交互に蹴れば登れます。
		addSolid({ -18.0f, 9.0f, -14.0f }, { 2.0f, 9.0f, 5.0f }, kKickWallColor, false);
		addSolid({ -10.0f, 9.0f, -14.0f }, { 2.0f, 9.0f, 5.0f }, kKickWallColor, false);

		// ゴールへの中継足場。ワイヤー練習台からゴールまでの経路を繋ぎます。
		addSolid({ 8.0f, 4.5f, 14.0f }, { 3.5f, 1.0f, 3.5f }, kBeamColor);

		// 練習の最後に目指す高台
		addSolid({ 16.0f, 4.0f, 20.0f }, { 5.0f, 4.0f, 5.0f }, kStructureColor);

		m_SpawnPoint = { 0.0f, 1.0f, -24.0f };
		m_TrainingPoints.Spawn = m_SpawnPoint;
		m_TrainingPoints.Guide = { 4.5f, 0.0f, -18.0f };
		m_TrainingPoints.WireAnchor = { 0.0f, 20.5f, 8.0f };
		m_TrainingPoints.FinishAnchor = { 12.0f, 14.0f, 17.0f };
		m_TrainingPoints.KickWallGap = { -14.0f, 0.0f, -14.0f };
		m_TrainingPoints.Finish = { 16.0f, 8.0f, 20.0f };
	}
	else
	{
    // 300mの開放型タワー。中央を塞がず、外周の足場と中央の速いルートを見通せます。
    addSolid({0,-2,0}, {48,2,48}, kFloorColor);
    m_SpawnPoint = {0,0.2f,-30};
    for (float x : {-40.0f, 40.0f})
        for (float z : {-40.0f, 40.0f})
            addSolid({x,155,z}, {3,155,3}, kCreamColor);

    // 外周の階段状ルート。12.5m刻み、各足場から次のクレーンが射程内です。
    for (int i = 1; i <= 23; ++i)
    {
        const float angle = -XM_PIDIV2 + (i - 1) * (XM_PI / 3.0f);
        const float height = i * 12.5f;
        const Vector3 p{cosf(angle) * 27.0f, height, sinf(angle) * 27.0f};
        const bool rest = i % 4 == 0;
        const Vector3 half{rest ? 10.0f : 7.0f, 1.0f, rest ? 10.0f : 7.0f};
        addSolid(p - Vector3{0,1,0}, half, rest ? kBeamColor : kConveyorColor);
        if (rest) m_HighPerchPoints.push_back(p);
        // クレーンの支柱と吊り梁。光は下面に置いて狙いやすくします。
        const Vector3 hook = p + Vector3{0,18,0};
        addSolid(p + Vector3{9,8,0}, {1.2f,10,1.2f}, kCreamColor);
        addSolid(hook + Vector3{3,1.5f,0}, {8,1.2f,2}, kStructureColor);
        m_RaceAnchors.push_back({hook,false});
        // 大型箱と青い壁で、着地後も壁蹴りから立て直せます。
        if (i % 3 == 0)
            addSolid(p + Vector3{-7,5,5}, {1.2f,6,5}, kKickWallColor);
        // 足場の外側の配管も実体のあるワイヤー対象です。
        addSolid(p + Vector3{0,-4,6}, {8,1.2f,1.2f}, kPowerColor);
    }
    // オレンジの中央クレーンは40m刻み。小さな着地点で外周を大きく短縮できます。
    for (int i = 0; i < 7; ++i)
    {
        const float y = 40.0f + i * 40.0f;
        const float x = i % 2 == 0 ? -10.0f : 10.0f;
        const float z = i % 2 == 0 ? 5.0f : -5.0f;
        addSolid({x,y+2,z}, {12,1.4f,2.2f}, kPowerColor);
        addSolid({x,y-18,z}, {3,1,3}, kPinkColor);
        m_RaceAnchors.push_back({{x,y,z},true});
    }
    // 復帰用の大きな看板。縦の隙間で落ちても横へワイヤーを掛け直せます。
    for (int i = 0; i < 5; ++i)
    {
        const float y = 35.0f + i * 55.0f;
        const float x = i % 2 == 0 ? -38.0f : 38.0f;
        addSolid({x,y,0}, {1.5f,6,12}, kPinkColor);
        m_RaceAnchors.push_back({{x > 0 ? x-2 : x+2,y,0},false});
    }
    // 頂上の着地台とチェッカーゲート。遠方からもシルエットを認識できます。
    addSolid({0,299,0}, {13,1,13}, kStructureColor);
    for (float x : {-13.0f,13.0f}) addSolid({x,309,0}, {1.8f,10,1.8f}, kPinkColor);
    for (int x = 0; x < 10; ++x)
        for (int y = 0; y < 2; ++y)
            addSolid({-13.5f+x*3.0f,319.0f+y*2.5f,0}, {1.5f,1.25f,2},
                (x+y)%2 == 0 ? kCreamColor : XMFLOAT4{0.08f,0.18f,0.26f,1});
    m_RaceAnchors.push_back({{0,317,-2.5f},false});
    }

    m_StaticIndexCount = static_cast<unsigned int>(indices.size());
    m_StaticField = m_BlockField;
    if (m_Layout == Layout::Factory)
    {
        // 予測できる周期運動。待って乗る安全策と、移動する吊り箱を使う短縮策。
        for (int i = 0; i < 5; ++i)
        {
            const Vector3 base{-22.0f, 42.0f+i*50.0f, 12.0f};
            const Vector3 half{5,1,5};
            const unsigned int first = static_cast<unsigned int>(indices.size());
            AddBox(vertices, indices, {}, half, kBeamColor);
            m_MovingBlocks.push_back({base,half,{0,10,0},base,0.7f,first});
        }
        for (int i = 0; i < 4; ++i)
        {
            const Vector3 base{16.0f,65.0f+i*60.0f,8.0f};
            const Vector3 half{3,3,3};
            const unsigned int first = static_cast<unsigned int>(indices.size());
            AddBox(vertices, indices, {}, half, kPinkColor);
            m_MovingBlocks.push_back({base,half,{9,0,9},base,0.6f,first});
        }
    }
	D3D11_BUFFER_DESC vertexDesc{};
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Engine::VERTEX_3D));
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
    for (auto& block : m_MovingBlocks)
    {
        block.Anchor = GetWorld().Spawn<WireAnchor>();
        block.Anchor->SetPosition(block.Position + Vector3{0,block.HalfSize.y+0.5f,0});
        block.Anchor->SetRadius(3);
        block.Anchor->SetColors({1,0.4f,0.7f,0.9f},{1,1,1,1});
    }
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

	Engine::Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Engine::Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());

	Engine::MATERIAL material{};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.Ambient = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = FALSE;
	Engine::Renderer::SetMaterial(material);

	UINT stride = sizeof(Engine::VERTEX_3D);
	UINT offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Engine::Renderer::GetDeviceContext()->DrawIndexed(m_StaticIndexCount, 0, 0);
    for (const auto& block : m_MovingBlocks)
    {
        Engine::Renderer::SetWorldMatrix(XMMatrixTranslation(block.Position.x, block.Position.y, block.Position.z));
        Engine::Renderer::GetDeviceContext()->DrawIndexed(36, block.FirstIndex, 0);
    }
    Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());

	GameObject::Draw();
}


void WireFactory::Update()
{
    m_MotionTime += Engine::GameTime::GetDeltaTime();
    m_BlockField = m_StaticField;
    WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
    for (auto& block : m_MovingBlocks)
    {
        const Vector3 previous = block.Position;
        const float phase = m_MotionTime * block.Speed;
        block.Position = block.Base + Vector3{
            sinf(phase)*block.Amplitude.x, sinf(phase)*block.Amplitude.y,
            (cosf(phase)-1.0f)*block.Amplitude.z};
        // 足場に立っている間は平行移動を受け継ぎます。ジャンプ後は運びません。
        if (player && player->IsOnGround())
        {
            const Vector3 p = player->GetPosition();
            if (std::abs(p.y-(previous.y+block.HalfSize.y)) < 0.3f
                && std::abs(p.x-previous.x) < block.HalfSize.x
                && std::abs(p.z-previous.z) < block.HalfSize.z)
                player->SetPosition(p + block.Position - previous);
        }
        m_BlockField.Add(block.Position, block.HalfSize);
        if (block.Anchor) block.Anchor->SetPosition(block.Position + Vector3{0,block.HalfSize.y+0.5f,0});
    }
    GameObject::Update();
}
