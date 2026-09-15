#pragma once

#include "gameObject.h"
#include "renderer.h"
#include "wireBlockField.h"

#include <vector>
class WireAnchor;

// 暴走したハイテク工場のステージ本体です。
//
// WireCityの手続き生成都市を置き換えるものですが、AABBのレイキャストと
// 押し出しはWireBlockFieldとして切り出して再利用しています。
// 設備そのもの(クレーン・配管・搬送機)はワイヤー対象として別オブジェクトで
// 生成し、ここは床・壁・大きな構造物の形状と描画だけを担当します。
class WireFactory : public GameObject
{
public:
	// 同じ衝突・描画の仕組みで、本編の工場と練習場を切り替えます。
	enum class Layout
	{
		Factory,
		Training
	};

	// 練習場で案内に使う位置です。形状と一緒にここで決め、シーン側で座標を重複させません。
	struct TrainingPoints
	{
		Vector3 Spawn{ 0.0f, 0.0f, 0.0f };
		Vector3 Guide{ 0.0f, 0.0f, 0.0f };
		Vector3 WireAnchor{ 0.0f, 0.0f, 0.0f };
		Vector3 FinishAnchor{ 0.0f, 0.0f, 0.0f };
		Vector3 KickWallGap{ 0.0f, 0.0f, 0.0f };
		Vector3 Finish{ 0.0f, 0.0f, 0.0f };
	};
	struct RaceAnchor { Vector3 Position; bool Shortcut = false; };

private:
	Layout m_Layout = Layout::Factory;
	TrainingPoints m_TrainingPoints;
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11Buffer> m_IndexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	unsigned int m_IndexCount = 0;

	WireBlockField m_BlockField;
	// テストステージの進行に使う代表的な位置です。ステージ側が配置の基準にします。
	Vector3 m_SpawnPoint{ 0.0f, 1.0f, 0.0f };
	std::vector<Vector3> m_HighPerchPoints;
	std::vector<RaceAnchor> m_RaceAnchors;
	struct MovingBlock
	{
		Vector3 Base, HalfSize, Amplitude, Position;
		float Speed;
		unsigned int FirstIndex;
		WireAnchor* Anchor = nullptr;
	};
	std::vector<MovingBlock> m_MovingBlocks;
	WireBlockField m_StaticField;
	unsigned int m_StaticIndexCount = 0;
	float m_MotionTime = 0.0f;

	void BuildGeometry();
	void AddBox(std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
		const Vector3& center, const Vector3& halfSize, const XMFLOAT4& color);

public:
	explicit WireFactory(Layout layout = Layout::Factory) : m_Layout(layout) {}

	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	const WireBlockField& GetBlockField() const { return m_BlockField; }
	const TrainingPoints& GetTrainingPoints() const { return m_TrainingPoints; }
	const std::vector<RaceAnchor>& GetRaceAnchors() const { return m_RaceAnchors; }

	Vector3 GetSpawnPoint() const { return m_SpawnPoint; }
	const std::vector<Vector3>& GetHighPerchPoints() const { return m_HighPerchPoints; }
};
