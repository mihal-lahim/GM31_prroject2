#pragma once

#include "gameObject.h"

class WirePlatform : public GameObject
{
private:
	bool m_Visible = true;

public:
	explicit WirePlatform(bool visible = true) : m_Visible(visible) {}

	void Init() override;
	void Uninit() override;
	void Update() override;

	float GetTopY() const;
	bool ContainsXZ(const Vector3& position, float margin) const;
};
