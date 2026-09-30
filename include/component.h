#pragma once



// GameObjectに所有される機能単位です。
// Component単体では寿命を管理せず、GameObjectのライフサイクルからのみ呼び出されます。
class Component
{

protected:

	class  GameObject* m_GameObject = nullptr;

public:
	Component() = delete;
	//Component() {}
	Component(GameObject* Object) { m_GameObject = Object; }
	virtual ~Component() {}

	virtual void Init() {};
	virtual void Uninit() {};
	// 入力・姿勢、ゲーム進行、後処理、描画の順にGameObjectが呼び出します。
	virtual void PreUpdate() {};
	virtual void Update() {};
	virtual void LateUpdate() {};
	virtual void Draw() {};
	// 影を落とす場合だけ実装します。Engine::ShadowMap::BindCaster を呼んでから形状を描きます。
	virtual void DrawShadow() {};
	// SetPosition/SetScale等の公開変換API経由で通知されます。
	virtual void OnOwnerTransformChanged() {};

};
