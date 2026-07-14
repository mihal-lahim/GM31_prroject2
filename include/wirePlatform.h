#pragma once

#include "gameObject.h"

class WirePlatform : public GameObject
{
private:
	bool m_LandingEnabled = true;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;

	float GetTopY() const;
	bool ContainsXZ(const Vector3& position, float margin) const;
	void SetLandingEnabled(bool enabled) { m_LandingEnabled = enabled; }
	bool IsLandingEnabled() const { return m_LandingEnabled; }
};
