#pragma once

#include "engineCommon.h"

namespace Engine
{
// Sceneの上へ黒い全画面ポリゴンを重ねる、共通の画面遷移表示です。
class SceneFade
{
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;

public:
	bool Init();
	void Uninit();
	void Draw(float alpha) const;
};
}
