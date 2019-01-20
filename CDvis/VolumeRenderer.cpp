#include "VolumeRenderer.hpp"
#include <CommandList.hpp>
#include <AssetDatabase.hpp>
#include <Camera.hpp>
#include <Graphics.hpp>

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

VolumeRenderer::VolumeRenderer() : VolumeRenderer(L"") {}
VolumeRenderer::VolumeRenderer(jwstring name) : Renderer(name), mDensity(1.0f), mLightDensity(3.0f), mVisible(true) {
	mCubeMesh = shared_ptr<Mesh>(new Mesh(L"Cube"));
	mCubeMesh->LoadCube(.5f);
	mCubeMesh->UploadStatic();
	mShader = AssetDatabase::GetAsset<Shader>(L"Volume");

	uint16_t white16[1] = { 0xFFFF };
	mTexture = shared_ptr<Texture>(new Texture(L"Blank Vol Texture", 1, 1, 1, D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R16_FLOAT, 1, white16, sizeof(uint16_t), false));
	mTexture->Upload(D3D12_RESOURCE_FLAG_NONE, false);
}
VolumeRenderer::~VolumeRenderer() {
	for (const auto &i : mSRVTables)
		delete[] i.second;
}

void VolumeRenderer::SetTexture(std::shared_ptr<Texture> tex) {
	mTexture = tex;
	for (const auto &it : mSRVTables)
		for (unsigned int i = 0; i < Graphics::BufferCount(); i++)
			it.second[i]->SetSRV(0, mTexture);
}

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

void VolumeRenderer::GatherRenderJobs(std::shared_ptr<CommandList> commandList, shared_ptr<Camera> camera, jvector<RenderJob*> &list) {
	if (!mCubeMesh || !mShader) return;
	UpdateTransform();

	shared_ptr<DescriptorTable>* arr;
	if (mSRVTables.count(camera.get())) {
		arr = mSRVTables.at(camera.get());
	} else {
		arr = new shared_ptr<DescriptorTable>[Graphics::BufferCount()];
		for (unsigned int i = 0; i < Graphics::BufferCount(); i++)
			arr[i] = shared_ptr<DescriptorTable>(new DescriptorTable(mName + L" SRV Table", 2));
		mSRVTables.emplace(camera.get(), arr);
	}

	shared_ptr<DescriptorTable> tbl = arr[commandList->GetFrameIndex()];
	shared_ptr<Texture> depthTex = tbl->GetTexture(1);
	if (!depthTex || depthTex->Width() != camera->PixelWidth() || depthTex->Height() != camera->PixelHeight()) {
		depthTex = shared_ptr<Texture>(new Texture(L"Volume Depth Texture", camera->PixelWidth(), camera->PixelHeight(), DXGI_FORMAT_R32_FLOAT));
		depthTex->Upload(D3D12_RESOURCE_FLAG_NONE, false);
		tbl->SetSRV(0, mTexture);
		tbl->SetSRV(1, depthTex);
	}

	list.push_back(new VolumeRenderJob(5000, this, camera.get(), tbl.get()));
}

void VolumeRenderer::VolumeRenderJob::Execute(shared_ptr<CommandList> commandList, shared_ptr<Material> materialOveride) {
	if (materialOveride) return;

	auto depthTex = mSRVTable->GetTexture(1)->GetTexture().Get();

	// copy depth texture
	if (mCamera->SampleCount() == 1) {
		commandList->TransitionResource(depthTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->TransitionResource(mCamera->DepthBuffer().Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->D3DCommandList()->CopyResource(depthTex, mCamera->DepthBuffer().Get());
		commandList->TransitionResource(mCamera->DepthBuffer().Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandList->TransitionResource(depthTex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	} else {
		commandList->TransitionResource(depthTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST);
		commandList->TransitionResource(mCamera->DepthBuffer().Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		commandList->D3DCommandList()->ResolveSubresource(depthTex, 0, mCamera->DepthBuffer().Get(), 0, DXGI_FORMAT_R32_FLOAT);
		commandList->TransitionResource(mCamera->DepthBuffer().Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandList->TransitionResource(depthTex, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	commandList->PushState();

	if (mVolume->mLightingEnable)
		commandList->EnableKeyword("LIGHTING");
	else
		commandList->DisableKeyword("LIGHTING");

	if (mVolume->mDepthColor)
		commandList->EnableKeyword("DEPTHCOLOR");
	else
		commandList->DisableKeyword("DEPTHCOLOR");

	if (mVolume->mSlicePlaneEnable)
		commandList->EnableKeyword("PLANE");
	else
		commandList->DisableKeyword("PLANE");

	XMFLOAT4X4 proj = mCamera->Projection();
	XMFLOAT4X4 viewproj = mCamera->ViewProjection();
	XMFLOAT3 campos = mCamera->WorldPosition();

	XMFLOAT3 lightDir;
	XMStoreFloat3(&lightDir, XMVector3TransformNormal(XMVector3Normalize(XMVectorSet(.1f, .1f, 1.0f, 1.0f)), XMLoadFloat4x4(&mVolume->WorldToObject())));

	// p' = transpose(inverse(M))*p
	XMVECTOR p = XMVector4Transform(XMLoadFloat4(&mVolume->mSlicePlane), XMMatrixTranspose(XMLoadFloat4x4(&mVolume->ObjectToWorld())));

	XMFLOAT4 slice;
	XMStoreFloat4(&slice, p);

	commandList->SetMaterial(nullptr);
	commandList->SetShader(mVolume->mShader);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 16, &mVolume->ObjectToWorld(), 0);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 16, &mVolume->WorldToObject(), 16);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 16, &viewproj, 32);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 3, &campos, 48);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 1, &mVolume->mDensity, 51);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 3, &lightDir, 52);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 1, &mVolume->mLightDensity, 55);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 4, &slice, 56);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 1, &proj._43, 60);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, 1, &proj._33, 61);

	commandList->SetRootDescriptorTable(1, mSRVTable->D3DHeap().Get(), mSRVTable->GpuDescriptor());

	commandList->SetBlendState(BLEND_STATE_ALPHA);
	commandList->SetCullMode(D3D12_CULL_MODE_FRONT);
	commandList->DepthTestEnabled(false);
	commandList->DepthWriteEnabled(false);
	commandList->DrawMesh(*mVolume->mCubeMesh);
	commandList->PopState();
}