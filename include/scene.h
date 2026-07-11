#pragma once

#include "world.h"

#include <functional>
#include <memory>
#include <utility>

// 1つのゲーム画面と、その画面専用のWorldをまとめる単位です。
class Scene
{
private:
	World m_World;
	std::function<void(std::unique_ptr<Scene>)> m_ChangeScene;

protected:
	template<typename T>
	void ChangeScene()
	{
		if (m_ChangeScene)
		{
			m_ChangeScene(std::make_unique<T>());
		}
	}

public:
	virtual ~Scene() = default;

	World& GetWorld() { return m_World; }
	const World& GetWorld() const { return m_World; }
	void SetSceneChangeHandler(std::function<void(std::unique_ptr<Scene>)> handler)
	{
		m_ChangeScene = std::move(handler);
	}

	// ManagerがScene切替境界で呼びます。Scene実装はWorldを直接利用します。
	virtual void Init() = 0;
	virtual void Uninit() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
};
