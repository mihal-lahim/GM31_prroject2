#pragma once

#include "gameObject.h"
#include "resourceManager.h"

#include <functional>
#include <string>

class WirePlayer;

// 工場に取り残された作業員です。
//
// 救助条件は作業員ごとに違うため、条件そのものを関数オブジェクトとして
// 外から渡す形にしています。「扉を壊す」「巨大機械を止める」「敵を全滅させる」
// といった条件を、Workerのコードを変えずに追加できます。
class WireWorker : public GameObject
{
public:
	// 解放条件です。trueを返すと近づいて救助できる状態になります。
	using UnlockCondition = std::function<bool()>;

private:
	static ComPtr<ID3D11Buffer> s_VertexBuffer;
	static ComPtr<ID3D11InputLayout> s_VertexLayout;
	static ComPtr<ID3D11VertexShader> s_VertexShader;
	static ComPtr<ID3D11PixelShader> s_PixelShader;
	static TextureHandle s_Texture;
	static int s_RefCount;

	UnlockCondition m_Condition;
	// 条件が未設定なら最初から解放済みとして扱います(到達するだけで救助)。
	bool m_Unlocked = true;
	bool m_Rescued = false;
	float m_Pulse = 0.0f;
	float m_RescueEffectTime = 0.0f;
	float m_RescueRadius = 4.5f;
	// 条件が未達のときにUIへ出す案内文です。A-Zと空白のみ使用します。
	std::string m_Hint;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	// 救助条件と案内文を設定します。conditionが空なら到達するだけで救助できます。
	void SetCondition(UnlockCondition condition, const std::string& hint);
	void SetRescueRadius(float radius) { m_RescueRadius = radius; }

	bool IsUnlocked() const { return m_Unlocked; }
	bool IsRescued() const { return m_Rescued; }
	const std::string& GetHint() const { return m_Hint; }

	// プレイヤーが救助圏内にいて条件も満たしていれば救助します。
	bool TryRescue(const WirePlayer& player);

	static void UnloadSharedResources();
};
