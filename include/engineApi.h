#pragma once

// Engine APIの段階的な名前空間移行用ヘッダーです。
// 既存ゲームとのソース互換性を保つため、従来のグローバル型は当面残し、
// 新規コードでは原則としてEngine::名前空間から参照します。

class Audio;
class Collider;
class CollisionWorld;
class Component;
class GameObject;
class GameTime;
class Manager;
class Renderer;
class RendererStateScope;
class RenderQueue;
class ResourceManager;
class World;

namespace Engine
{
	using Audio = ::Audio;
	using Collider = ::Collider;
	using CollisionWorld = ::CollisionWorld;
	using Component = ::Component;
	using GameObject = ::GameObject;
	using GameTime = ::GameTime;
	using Manager = ::Manager;
	using Renderer = ::Renderer;
	using RendererStateScope = ::RendererStateScope;
	using RenderQueue = ::RenderQueue;
	using ResourceManager = ::ResourceManager;
	using World = ::World;
}
