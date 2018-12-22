#include "VolumeRenderer.hpp"
#include <CommandList.hpp>
#include <Camera.hpp>

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

VolumeRenderer::VolumeRenderer() : VolumeRenderer(L"") {}
VolumeRenderer::VolumeRenderer(jwstring name) : Renderer(name) {}
VolumeRenderer::~VolumeRenderer() {}

DirectX::BoundingOrientedBox VolumeRenderer::Bounds() {
	if (mCubeMesh) {
		XMFLOAT3 mcenter = mCubeMesh->Bounds().Center;
		XMFLOAT3 mbounds = mCubeMesh->Bounds().Extents;
		XMFLOAT3 scale = WorldScale();
		XMVECTOR p = XMVector3Transform(XMLoadFloat3(&mcenter), XMLoadFloat4x4(&ObjectToWorld()));
		XMStoreFloat3(&mcenter, p);
		return BoundingOrientedBox(mcenter, XMFLOAT3(mbounds.x * scale.x, mbounds.y * scale.y, mbounds.z * scale.z), WorldRotation());
	}
	return BoundingOrientedBox(WorldPosition(), XMFLOAT3(0, 0, 0), WorldRotation());
}

void VolumeRenderer::Draw(shared_ptr<CommandList> commandList) {
	if (!mCubeMesh || !mShader) return;
	UpdateTransform();

	XMFLOAT4X4 vp = commandList->GetCamera()->ViewProjection();
	XMFLOAT3 cp = commandList->GetCamera()->WorldPosition();

	commandList->SetShader(mShader);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 16, &ObjectToWorld(), 0);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 16, &vp, 16);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 4, &cp, 32);
	commandList->DrawMesh(*mCubeMesh);
}