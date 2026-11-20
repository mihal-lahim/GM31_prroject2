#include "renderQueue.h"

#include "cameraView.h"
#include "renderer.h"
#include "shadowMap.h"
#include "world.h"

#include <algorithm>
#include <cmath>

void RenderQueue::Build(World& world, const ICameraView* camera)
{
	Clear();
	m_Items.reserve(world.GetObjects().size());
	if (camera != nullptr)
	{
		const Vector3 position = camera->GetCameraPosition();
		const Vector3 forward = camera->GetCameraForward();
		// 影の範囲はカメラの足元ではなく、見ている方向へ少し進んだ辺りを中心にします。
		Vector3 flatForward{ forward.x, 0.0f, forward.z };
		flatForward.normalize();
		m_ShadowFocus = position + flatForward * (Engine::ShadowMap::kCoverage * 0.3f);
		m_HasShadowFocus = true;
		for (auto& gameObject : world.GetObjects())
		{
			// Overlayは投入順ソートなので深度計算は不要です。
			if (gameObject->GetRenderCategory() != RenderCategory::Overlay)
			{
				gameObject->CalcCameraZ(position, forward);
			}
		}
	}

	for (auto& gameObject : world.GetObjects())
	{
		Submit(*gameObject);
	}
	Sort();
}

void RenderQueue::Submit(GameObject& object)
{
	m_Items.push_back({
		&object,
		object.GetLayer(),
		object.GetRenderCategory(),
		object.GetCameraZ(),
		m_Items.size()
	});
}

void RenderQueue::Sort()
{
	std::sort(m_Items.begin(), m_Items.end(), [](const RenderItem& left, const RenderItem& right)
	{
		if (left.ObjectLayer != right.ObjectLayer)
		{
			return static_cast<int>(left.ObjectLayer) < static_cast<int>(right.ObjectLayer);
		}
		if (left.Category != right.Category)
		{
			return static_cast<int>(left.Category) < static_cast<int>(right.Category);
		}
		// 深度が同値のときは投入順へ落とします。std::sortは非安定なので、
		// ここで順序を決めないと同一平面のポリゴンがフレームごとに入れ替わります。
		const bool depthSorted = left.Category == RenderCategory::Transparent
			|| left.Category == RenderCategory::Opaque;
		if (depthSorted)
		{
			// NaNはどの値とも比べられず、std::sortの前提(狭義の弱順序)が崩れて落ちます。
			// NaNの深度は有限の深度の後ろへまとめ、NaNどうしは投入順にします。
			const bool leftNaN = std::isnan(left.CameraDepth);
			const bool rightNaN = std::isnan(right.CameraDepth);
			if (leftNaN != rightNaN)
			{
				return rightNaN;
			}
			if (!leftNaN && left.CameraDepth != right.CameraDepth)
			{
				return left.Category == RenderCategory::Transparent
					? left.CameraDepth > right.CameraDepth
					: left.CameraDepth < right.CameraDepth;
			}
		}
		return left.SubmissionOrder < right.SubmissionOrder;
	});
}

void RenderQueue::Execute()
{
	if (m_Items.empty())
	{
		return;
	}

	// 各スコープは直前スコープの復元後=同一ベースラインを再取得していただけなので、
	// スナップショットは1回だけ取得し、各Draw後に同じ状態へ復元します。
	std::size_t index = 0;
	{
		Engine::RendererStateScope stateScope;
		auto draw = [&stateScope](const RenderItem& item)
		{
			if (item.Object == nullptr)
			{
				return;
			}
			item.Object->RunDraw();
			stateScope.Restore();
		};

		// カメラ層が先頭に並びます。カメラがライトと視点を設定してから影を描き、その後で通常の描画をします。
		for (; index < m_Items.size() && m_Items[index].ObjectLayer == Layer::Camera; ++index)
		{
			draw(m_Items[index]);
		}
		if (m_HasShadowFocus)
		{
			ExecuteShadowPass(index);
			// 影パスは描画先・視点・ラスタライザを切り替えるので、元のベースラインへ戻します。
			stateScope.Restore();
		}
		// UI より前(ワールド・エフェクト)を描きます。
		for (; index < m_Items.size() && m_Items[index].ObjectLayer != Layer::UI; ++index)
		{
			draw(m_Items[index]);
		}
	}
	if (index >= m_Items.size())
	{
		return;
	}

	// HUD の文字がブルームや歪みを受けないよう、ポスト効果を先に画面へ解決してから UI を描きます。
	// 解決後は描画先がバックバッファなので、スナップショットはここで取り直します(前のスコープへは戻しません)。
	Engine::Renderer::ResolvePostEffects();
	Engine::RendererStateScope uiScope;
	for (; index < m_Items.size(); ++index)
	{
		if (m_Items[index].Object != nullptr)
		{
			m_Items[index].Object->RunDraw();
			uiScope.Restore();
		}
	}
}

void RenderQueue::ExecuteShadowPass(std::size_t firstNonCameraItem)
{
	if (!Engine::ShadowMap::Begin(m_ShadowFocus))
	{
		return;
	}
	for (std::size_t index = firstNonCameraItem; index < m_Items.size(); ++index)
	{
		const RenderItem& item = m_Items[index];
		// 画面座標のUIは影を落としません。
		if (item.Object != nullptr && item.ObjectLayer != Layer::UI)
		{
			item.Object->RunDrawShadow();
		}
	}
	Engine::ShadowMap::End();
}
