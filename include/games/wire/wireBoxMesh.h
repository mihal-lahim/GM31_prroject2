#pragma once

#include "renderer.h"

#include <vector>

// 軸平行ボックスの頂点・インデックスを生成する共通ヘルパーです。
//
// 工場・巨大機械の部位・ロック機構がそれぞれ同じ生成コードを複製していて、
// 側面4つの巻き順が裏向き(外から見るとカリングされ、箱の内側が透けて見える)
// という不具合も3箇所に複製されていました。ここに集約し、巻き順は
// 面の基底の向きから機械的に決めるので、手で並べた表が間違っていても壊れません。
namespace WireBoxMesh
{
	// 6面分の頂点とインデックスを末尾に追加します。
	// colorに面ごとの陰影を掛けて頂点色へ焼き込みます。
	// shadeOnlyがtrueのときは色を白として陰影だけを焼き込み、
	// 描画時のマテリアル色で着色できるようにします(部位や扉の状態変化用)。
	void Append(std::vector<Engine::VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
		const Vector3& center, const Vector3& halfSize, const XMFLOAT4& color,
		bool shadeOnly = false);
}
