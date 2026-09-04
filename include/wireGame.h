#pragma once

#include "scene.h"

#include <vector>

class WireAnchor;
class WireDeliveryHud;

class WireGame : public Scene
{
private:
	Vector3 m_RespawnPosition{};
	std::vector<WireAnchor*> m_DeliveryMarkers;
	std::vector<bool> m_Delivered;
	WireDeliveryHud* m_Hud = nullptr;
	float m_ElapsedSeconds = 0.0f;
	float m_ResultDelay = 0.0f;
	bool m_Completed = false;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
