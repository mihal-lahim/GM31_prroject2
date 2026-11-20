#include"main.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "sky.h"
#include "cameraView.h"
#include "world.h"


void Sky::Init()
{
	m_Layer = Layer::World;
	AddComponent<ModelRenderer>()->Load("assets\\model\\sky.obj");

	SetScale({ kDefaultRadius, kDefaultRadius, kDefaultRadius });
}

void Sky::Uninit()
{
	GameObject::Uninit();
}

void Sky::SyncToCamera()
{
	// 通常カメラとTPSカメラのどちらでも追従できるよう、共通インターフェースを利用します。
	ICameraView* camera = GetWorld().FindFirst<ICameraView>();
	if (camera != nullptr)
	{
		const Vector3 cameraPosition = camera->GetCameraPosition();
		// 位置を変えると変換の通知が飛ぶので、変わったときだけ書きます。
		if (GetPosition() != cameraPosition)
		{
			SetPosition(cameraPosition);
		}
	}
}

void Sky::LateUpdate()
{
	SyncToCamera();
	GameObject::LateUpdate();
}

void Sky::Draw()
{
	// LateUpdateがカメラより先に走った場合も、このフレームのカメラの位置で描きます。
	SyncToCamera();

	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(GetScale().x, GetScale().y, GetScale().z);
	rot = XMMatrixRotationRollPitchYaw(GetRotation().x, GetRotation().y, GetRotation().z);
	trans = XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);
	world = scale * rot * trans;

	Engine::Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}

