#include "VRButton.hpp"

#include <AssetDatabase.hpp>
#include <Shader.hpp>
#include <Material.hpp>
#include <Scene.hpp>

using namespace std;
using namespace DirectX;

VRButton::VRButton(jwstring name, float size) : Object(name), mSize(size) {}
VRButton::~VRButton() {}

void VRButton::Init(shared_ptr<Texture> icon, XMFLOAT4 uv_st) {
	auto mat = shared_ptr<Material>(new Material(mName + L" Material", AssetDatabase::GetAsset<Shader>(L"textured")));
	mat->SetTexture("Texture", icon, -1);
	mat->SetFloat4("TextureST", uv_st, -1);
	mat->RenderQueue(2000);
	auto mesh = shared_ptr<Mesh>(new Mesh(mName + L" Mesh"));
	mesh->LoadQuad(mSize);
	mesh->UploadStatic();
	mRenderer = GetScene()->AddObject<MeshRenderer>(mName + L" Renderer");
	mRenderer->SetMesh(mesh);
	mRenderer->SetMaterial(mat);
}