#pragma once

#include "gameObject.h"
#include "renderer.h"

#include <vector>

// 剣が通った跡の光の帯(剣閃)です。プレイヤーが毎フレーム、剣の根元と切っ先を AddSample で渡し、
// 60Hz の間は Catmull-Rom で滑らかにつないで描きます。古い跡は実時間で薄れて消えます。
// 攻撃の振りの間だけ追加するので、振り終わると跡は自然に消えていきます。編集モードでは消します。
class SlowSwordTrail final : public GameObject
{
public:
	void Init() override;
	void Update() override;
	void Uninit() override;
	void Draw() override;

	// 1フレーム分の剣の根元と切っ先です。colorは根元側の基準の色で、切っ先側は明るく描きます。
	void AddSample(const Vector3& root, const Vector3& tip, const XMFLOAT4& color);
	// 次の振りの跡を始めます。前の振りの跡とは繋げず、振りごとに別々の帯にします。
	void BeginStroke() { ++m_Stroke; }
	// 跡をすべて消します。編集モードに入ったときや、シーンを終えるときに使います。
	void Clear() { m_Samples.clear(); }
	int GetSampleCount() const { return static_cast<int>(m_Samples.size()); }

private:
	struct Sample
	{
		Vector3 Root{ 0.0f, 0.0f, 0.0f };
		Vector3 Tip{ 0.0f, 0.0f, 0.0f };
		XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		// 追加されてからの実時間(ヒットストップ中は止まります)です。
		float Age = 0.0f;
		// 同じ振りの跡なら同じ番号です。番号が違う隣どうしは繋ぎません。
		int Stroke = 0;
	};

	std::vector<Sample> m_Samples;
	int m_Stroke = 0;
	std::vector<Engine::VERTEX_3D> m_Vertices;
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
};
