#pragma once

#include "Vector3.h"

#include <cstdint>
#include <filesystem>

class World;

// 実行中のWorldを覗いて書き換える、最小限のエディタUIです(階層・インスペクタ・保存)。
//
// ImGuiに依存するのでEngine.libには入れず、使うアプリが sceneEditor.cpp と third_party/imgui を
// 自分のプロジェクトでコンパイルします。エンジン側の Engine::SceneSerializer と
// GameObject::Reflect だけに依存し、特定のゲームの型は知りません。
//
// 使い方:
//   1. GameDefinitionのUIフックへ InitializeUi / ShutdownUi / BeginUiFrame / DrawUi / HandleWindowMessage を渡す。
//   2. SceneのInitで SetWorld と SetScenePaths を呼び、Uninitで SetWorld(nullptr) を呼ぶ。
//   3. 編集に入るとき SetActive(true)、戻るとき SetActive(false)。
//      編集中にゲームの進行を止めるのと、GetCamera() の視点で描くのはアプリ側の役目です。
namespace SceneEditor
{
	// 編集用の自由カメラです。forward = (sin(Yaw)cos(Pitch), -sin(Pitch), cos(Yaw)cos(Pitch))。
	struct CameraPose
	{
		Vector3 Position{ 0.0f, 0.0f, 0.0f };
		float Yaw = 0.0f;
		// 正の値で下を向きます。
		float Pitch = 0.0f;
		float FovDegrees = 60.0f;
	};

	bool InitializeUi();
	void ShutdownUi();
	void BeginUiFrame();
	void DrawUi();
	bool HandleWindowMessage(unsigned int message, std::uintptr_t wParam, std::intptr_t lParam);

	// 編集対象のWorldです。Sceneが破棄される前に必ずnullptrへ戻してください。
	void SetWorld(World* world);
	// runtimePathは実行フォルダ基準の保存先です。sourcePathが空でなければ、そこへも同じ内容を書きます。
	// ビルドのたびに実行フォルダのassetsは作り直されるので、開発中はソース側へも書かないと編集が残りません。
	void SetScenePaths(const std::filesystem::path& runtimePath, const std::filesystem::path& sourcePath);

	void SetActive(bool active);
	bool IsActive();
	// 文字入力中はtrueです。ゲーム側のキー操作(Rでやり直し等)を止める判定に使います。
	bool WantsKeyboard();

	// インスペクタに表示するオブジェクトをIDで選びます。0で選択解除です。
	void Select(std::uint64_t objectId);

	const CameraPose& GetCamera();
	void SetCamera(const CameraPose& pose);

	// 実行フォルダから親をたどり、markerFileのあるフォルダ(=ソースツリーの根)を探して、
	// そこからのrelativePathを返します。見つからなければ空のパスです(配布後の実行など)。
	std::filesystem::path FindSourcePath(const std::filesystem::path& relativePath, const char* markerFile);
}
