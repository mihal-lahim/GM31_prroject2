#pragma once

#include "engineCommon.h"

// Rendering and shared effects depend on this interface instead of a
// game-specific camera implementation.
class ICameraView
{
public:
	virtual ~ICameraView() = default;

	virtual XMMATRIX GetViewMatrix() const = 0;
	virtual Vector3 GetCameraPosition() const = 0;
	virtual Vector3 GetCameraForward() const = 0;
};
