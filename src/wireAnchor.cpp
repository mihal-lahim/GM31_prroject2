#include "main.h"
#include "wireAnchor.h"

#include "renderer.h"
#include "modelRenderer.h"

void WireAnchor::Init()
{
	m_Layer = Layer::World;
	m_Radius = 1.6f;
	ModelRenderer* renderer = AddComponent<ModelRenderer>(this);
	renderer->Load("assets\\model\\box.obj");
	renderer->SetUseOwnerTransform(true);
}

void WireAnchor::Uninit()
{
	GameObject::Uninit();
}

void WireAnchor::Update()
{
	GameObject::Update();
}
