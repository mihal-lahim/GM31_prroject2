#pragma once

#include "engineCommon.h"
#include "wireRopeVisual.h"

class WirePlayer;
class WireTpsCamera;

class WirePlayerPresentation
{
private:
	ComPtr<ID3D11Buffer> m_WireVertexBuffer;
	ComPtr<ID3D11InputLayout> m_LineVertexLayout;
	ComPtr<ID3D11VertexShader> m_LineVertexShader;
	ComPtr<ID3D11PixelShader> m_LinePixelShader;
	bool m_RenderingAvailable = false;
	// 速度線の発生数を蓄積し、低速でも一定間隔で線が出るようにします。
	float m_SpeedLineAccumulator = 0.0f;
	// 支点から手までの区間の、たるみと揺れです。見た目だけで、移動には影響しません。
	WireRopeVisual m_Rope;
	bool m_RopeActive = false;
	int m_RopeBendCount = 0;

	void UpdateRope(const WirePlayer& player, float dt);

public:
	void Init();
	void Uninit();
	// 速度に応じた演出を更新します。描画物を持たない演出はここで発生させます。
	void Update(const WirePlayer& player, float dt, const WireTpsCamera* camera);
	void Draw(const WirePlayer& player);
};
