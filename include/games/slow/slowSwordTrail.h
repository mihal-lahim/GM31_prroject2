#pragma once

#include "gameObject.h"
#include "renderer.h"

#include <vector>

// 斬り抜きの軌跡(スラッシュアーク)です。斬り抜きの瞬間に、プレイヤーの胸あたりを中心とした大きな三日月型の円弧を
// 1本出します。先端(ヘッド)が素早く伸び切ったあと、尾(テール)が追いかけて細く消えていきます。
// 手首の細かな動きには追従しないので、軌跡が折れ曲がらず、大きく派手に見えます。
// 円弧は出た場所に残り、実時間で動きます(ヒットストップ中は止まります)。編集モード・シーンの終わりでは全部消します。
class SlowSwordTrail final : public GameObject
{
public:
	// 円弧の形と色です。段と技ごとに、プレイヤーが決めて渡します。
	struct ArcStyle
	{
		// 展開角(度)。回り切る角度です。
		float SweepDegrees = 165.0f;
		// 中心からの半径(m)です。
		float Radius = 2.0f;
		// 弧の面の傾き(ラジアン)。0で縦(頭の上を通る)、π/2で水平です。
		float Tilt = 1.57f;
		// 回り方向です。+1と-1で、弧が左から右へ、右から左へ伸びます。
		float Direction = 1.0f;
		// 主の色です。中ほどの帯と、先端の白い縁の色に使います。
		XMFLOAT3 Color{ 0.85f, 0.95f, 1.0f };
	};

	void Init() override;
	void Update() override;
	void Uninit() override;
	void Draw() override;

	// 斬り抜きの瞬間に、center(胸あたり)を中心に1本の円弧を出します。yawは出したときのプレイヤーの向きです。
	void SpawnArc(const Vector3& center, float yaw, const ArcStyle& style);
	// 全部の円弧を消します。編集モードに入ったとき、やり直し、シーンの終わりに使います。
	void Clear() { m_Arcs.clear(); }
	int GetArcCount() const { return static_cast<int>(m_Arcs.size()); }

private:
	struct Arc
	{
		Vector3 Center{ 0.0f, 0.0f, 0.0f };
		// 弧の面の2本の基底です。Aは正面、Bは面に直交する向きです。
		Vector3 A{ 0.0f, 0.0f, 1.0f };
		Vector3 B{ 1.0f, 0.0f, 0.0f };
		ArcStyle Style;
		// 出てからの実時間です。
		float Age = 0.0f;
	};

	std::vector<Arc> m_Arcs;
	std::vector<Engine::VERTEX_3D> m_Vertices;
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
};
