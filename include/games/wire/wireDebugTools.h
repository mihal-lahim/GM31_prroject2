#pragma once

// Wire Actionの審査・検証用デバッグ機能です。
// Releaseビルドでも使用できますが、既定では表示・効果ともにOFFで、
// F1を押すまで通常プレイの画面には一切現れません。
namespace WireDebugTools
{
	// F7〜F10で要求されたシーン移動です。
	// SceneのChangeSceneはprotectedなので、移動そのものは各Sceneが行います。
	enum class SceneRequest
	{
		None,
		Title,
		Game,
		ResultCleared,
		ResultGameOver
	};

	// 全シーン共通のキー処理です。各SceneのUpdate冒頭から毎フレーム1回だけ呼びます。
	// 表示トグルや無敵などはこの中で処理し、シーン移動だけを戻り値で返します。
	SceneRequest Update();

	// F1で切り替わるデバッグ表示の状態です。
	bool IsOverlayVisible();

	// F4のデバッグ無敵です。WireGameがプレイヤーへ反映します。
	bool IsInvincibleEnabled();

	// F5で切り替わるタイムスケールの表示用の値です。
	float GetTimeScale();

	// ゲーム開始時に呼び、前回のプレイで有効化した設定を持ち越さないようにします。
	void ResetForNewGame();

	// 画面に出す操作ヘルプです。
	const char* GetHelpLine1();
	const char* GetHelpLine2();
}
