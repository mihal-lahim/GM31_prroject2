#pragma once

#include "gameTime.h"
#include "manager.h"
#include "renderer.h"
#include "resourceManager.h"

// Engine APIの段階的な名前空間移行用ヘッダーです。
// 既存ゲームとのソース互換性を保つため、従来のグローバル型は当面残し、
// 新規コードでは原則としてEngine::名前空間から参照します。

class Audio;
class Collider;
class CollisionWorld;
class Component;
class GameObject;
class RenderQueue;
class World;

namespace Engine
{
	using Audio = ::Audio;
	using Collider = ::Collider;
	using CollisionWorld = ::CollisionWorld;
	using Component = ::Component;
	using GameObject = ::GameObject;
	using RenderQueue = ::RenderQueue;
	using World = ::World;
}
