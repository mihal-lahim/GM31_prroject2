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

void Sky::Update()
{
	// 通常カメラとTPSカメラのどちらでも追従できるよう、共通インターフェースを利用します。
	ICameraView* camera = GetWorld().FindFirst<ICameraView>();
	if (camera != nullptr)
	{
		SetPosition(camera->GetCameraPosition());
	}

	GameObject::Update();
}

void Sky::Draw()
{
	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(GetScale().x, GetScale().y, GetScale().z);
	rot = XMMatrixRotationRollPitchYaw(GetRotation().x, GetRotation().y, GetRotation().z);
	trans = XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);
	world = scale * rot * trans;

	Engine::Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}

