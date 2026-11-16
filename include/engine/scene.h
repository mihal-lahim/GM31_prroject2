#pragma once

#include "world.h"

#include <functional>
#include <memory>
#include <utility>

// Scene切替の演出です。秒数0はその向きのフェードを省きます。
struct SceneTransition
{
	float FadeOutSeconds = 0.45f;
	float FadeInSeconds = 0.45f;

	static constexpr SceneTransition Default() { return {}; }
	// リトライのように何度も繰り返す切替向けです。暗転を挟まずに次の画面を出します。
	static constexpr SceneTransition Instant() { return { 0.0f, 0.0f }; }
	static constexpr SceneTransition Quick() { return { 0.1f, 0.15f }; }
};

// 1つのゲーム画面と、その画面専用のWorldをまとめる単位です。
class Scene
{
public:
	using SceneChangeHandler = std::function<void(std::unique_ptr<Scene>, SceneTransition)>;

private:
	World m_World;
	SceneChangeHandler m_ChangeScene;

protected:
	template<typename T, typename... Args>
	void ChangeScene(Args&&... args)
	{
		ChangeSceneWith<T>(SceneTransition::Default(), std::forward<Args>(args)...);
	}

	template<typename T, typename... Args>
	void ChangeSceneWith(SceneTransition transition, Args&&... args)
	{
		if (m_ChangeScene)
		{
			m_ChangeScene(std::make_unique<T>(std::forward<Args>(args)...), transition);
		}
	}

	// アプリを終了します。Escで終了させるかどうかはゲーム側が決めます。
	static void RequestQuit();

public:
	virtual ~Scene() = default;

	World& GetWorld() { return m_World; }
	const World& GetWorld() const { return m_World; }
	void SetSceneChangeHandler(SceneChangeHandler handler)
	{
		m_ChangeScene = std::move(handler);
	}
	// テスト向けの簡易版です。演出の指定は捨てます。
	void SetSceneChangeHandler(std::function<void(std::unique_ptr<Scene>)> handler)
	{
		m_ChangeScene = [handler = std::move(handler)](std::unique_ptr<Scene> next, SceneTransition)
		{
			handler(std::move(next));
		};
	}

	// ManagerがScene切替境界で呼びます。Scene実装はWorldを直接利用します。
	virtual void Init() = 0;
	virtual void Uninit() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
};
