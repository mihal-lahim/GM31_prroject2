#pragma once

#include "engineCommon.h"

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

public:
	void Init();
	void Uninit();
	// 速度に応じた演出を更新します。描画物を持たない演出はここで発生させます。
	void Update(const WirePlayer& player, float dt, const WireTpsCamera* camera);
	void Draw(const WirePlayer& player);
};
