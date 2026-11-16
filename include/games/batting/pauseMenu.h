#pragma once

#include "gameObject.h"

class PauseMenu : public GameObject
{
private:
	enum class DragTarget
	{
		None,
		Volume,
		Sensitivity
	};

	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	DragTarget m_DragTarget = DragTarget::None;

	void DrawRect(float x, float y, float width, float height, const XMFLOAT4& color);
	void DrawSlider(float x, float y, float width, float value, const XMFLOAT4& color);
	void DrawVolumeIcon(float x, float y);
	void DrawSensitivityIcon(float x, float y);
	bool IsMouseInRect(float mouseX, float mouseY, float x, float y, float width, float height) const;
	float SliderValueFromMouse(float mouseX, float x, float width) const;

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
