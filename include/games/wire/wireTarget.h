#pragma once

#include "Vector3.h"

class GameObject;
class World;
class WireTpsCamera;

// ワイヤーを掛けられる対象が実装するインターフェースです。
//
// 以前はWireCityのビル境界だけがワイヤー対象で、対象の種類がコードに直接
// 埋め込まれていました。工場設備・クレーン・巨大機械の部位など、動くものにも
// 掛けたいので、対象側がこのインターフェースを実装する形へ反転させています。
// 追加時にワイヤー側のコードを触る必要はありません。
class IWireTarget
{
public:
	virtual ~IWireTarget() = default;

	// フックが刺さる位置です。動く設備は毎フレーム違う値を返して構いません。
	virtual Vector3 GetWireAnchorPosition() const = 0;

	// 装甲が壊れた、電源が落ちたなど、一時的に掛けられなくなる対象があるためのフラグです。
	virtual bool IsWireTargetActive() const = 0;

	// 狙いの吸着半径です。大きな設備ほど狙いやすくなります。
	virtual float GetWireTargetRadius() const = 0;

	// 表示や無効化のためにGameObjectとして参照したい場面があるため公開します。
	virtual GameObject* GetWireTargetObject() = 0;
};

// 狙いを解決した結果です。Targetがnullptrの場合はステージ形状への直接ヒットで、
// その場合でもAnchorPositionは有効です。
struct WireTargetHit
{
	Vector3 AnchorPosition{ 0.0f, 0.0f, 0.0f };
	IWireTarget* Target = nullptr;
	float Distance = 0.0f;
	bool Valid = false;
};

namespace WireTargeting
{
	// カメラの視線から最良のワイヤー対象を選びます。
	//
	// 厳密なレイキャストではなく、視線コーン内の候補をスコアで選ぶ方式です。
	// 高速移動中にピクセル単位で狙わせないための入力補正であり、
	// 「多少補正してでもスタイリッシュに動けること」を優先する方針に沿っています。
	WireTargetHit FindBestTarget(World& world, const WireTpsCamera& camera);

	// 指定した対象が今もWorldに存在し、掛けられる状態かを確認します。
	// 破棄された対象を掴んだままにしないため、接続中は毎フレーム呼びます。
	bool IsTargetStillValid(World& world, IWireTarget* target);
}
