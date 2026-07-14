#pragma once

#include "gameObject.h"

class WirePlatform : public GameObject
{
public:
	void Init() override;
	void Uninit() override;
	void Update() override;

	float GetTopY() const;
	bool ContainsXZ(const Vector3& position, float margin) const;
};
