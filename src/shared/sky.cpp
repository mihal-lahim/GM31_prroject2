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

	m_Scale = { kDefaultRadius, kDefaultRadius, kDefaultRadius };
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
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}

