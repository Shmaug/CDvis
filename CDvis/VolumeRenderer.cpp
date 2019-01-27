#include "VolumeRenderer.hpp"
#include <CommandList.hpp>
#include <AssetDatabase.hpp>
#include <Camera.hpp>
#include <Graphics.hpp>

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

VolumeRenderer::VolumeRenderer() : VolumeRenderer(L"") {}
VolumeRenderer::VolumeRenderer(jwstring name) : Renderer(name), mDensity(1.5f), mLightDensity(35.0f), mExposure(1.25f), mVisible(true) {
	mCubeMesh = shared_ptr<Mesh>(new Mesh(L"Cube"));
	mCubeMesh->LoadCube(.5f);
	mCubeMesh->UploadStatic();
	mShader = AssetDatabase::GetAsset<Shader>(L"volume");

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

float VolumeRenderer::GetPixel(XMUINT3 p) const {
	if (p.x < 0 || p.y < 0 || p.z < 0 || p.x >= mTexture->Width() || p.y >= mTexture->Height() || p.z >= mTexture->Depth()) return 0.0f;
	uint16_t* pixels = (uint16_t*)mTexture->GetPixelData();
	return (float)pixels[p.x + p.y * mTexture->Width() + p.z * (mTexture->Width() * mTexture->Height())] / (float)0xFFFF;
}

float VolumeRenderer::GetDensity(XMFLOAT3 uvw, bool checkPlane) const {
	if (checkPlane && mSlicePlaneEnable) {
		XMVECTOR p = XMVectorSubtract(XMLoadFloat3(&uvw), XMVectorSet(.5f, .5f, .5f, 0.f));
		if (XMVectorGetX(XMVector3Dot(XMVectorSubtract(p, XMLoadFloat3(&mSlicePlanePoint)), XMLoadFloat3(&mSlicePlaneNormal))) < 0)
			return 0.f;
	}
	return GetPixel({
		(unsigned int)(uvw.x * mTexture->Width()),
		(unsigned int)(uvw.y * mTexture->Height()),
		(unsigned int)(uvw.z * mTexture->Depth())
	});
}
float VolumeRenderer::GetDensityTrilinear(XMFLOAT3 uvw, bool checkPlane) const {
	if (checkPlane && mSlicePlaneEnable) {
		XMVECTOR p = XMVectorSubtract(XMLoadFloat3(&uvw), XMVectorSet(.5f, .5f, .5f, 0.f));
		if (XMVectorGetX(XMVector3Dot(XMVectorSubtract(p, XMLoadFloat3(&mSlicePlanePoint)), XMLoadFloat3(&mSlicePlaneNormal))) < 0)
			return 0.f;
	}

	uvw.x *= mTexture->Width();
	uvw.y *= mTexture->Height();
	uvw.z *= mTexture->Depth();

	float fx = uvw.x - (int)uvw.x;
	float fy = uvw.y - (int)uvw.y;
	float fz = uvw.z - (int)uvw.z;

	XMUINT3 p { (unsigned int)uvw.x, (unsigned int)uvw.y, (unsigned int)uvw.z };

	// Get corner densities
	float c000 = GetPixel({ p.x,	 p.y,	  p.z });
	float c100 = GetPixel({ p.x + 1, p.y,	  p.z });
	float c010 = GetPixel({ p.x,	 p.y + 1, p.z });
	float c110 = GetPixel({ p.x + 1, p.y + 1, p.z });

	float c001 = GetPixel({ p.x,	 p.y,	  p.z + 1 });
	float c101 = GetPixel({ p.x + 1, p.y,	  p.z + 1 });
	float c011 = GetPixel({ p.x,	 p.y + 1, p.z + 1 });
	float c111 = GetPixel({ p.x + 1, p.y + 1, p.z + 1 });

	// Flatten cube to plane on yz axis by interpolating densities along the x axis
	float c00 = c000 * (1.0f - fx) + c100 * fx;
	float c01 = c001 * (1.0f - fx) + c101 * fx;

	float c10 = c010 * (1.0f - fx) + c110 * fx;
	float c11 = c011 * (1.0f - fx) + c111 * fx;

	// Flatten plane to line on z axis by interpolating densities along y axis
	float c0 = c00 * (1.0f - fy) + c10 * fy;
	float c1 = c01 * (1.0f - fy) + c11 * fy;

	// Flatten line to point by interpolating densities along z axis
	return c0 * (1.0f - fz) + c1 * fz;
}

void VolumeRenderer::SetSlicePlane(DirectX::XMFLOAT3 p, DirectX::XMFLOAT3 n) {
	// p' = transpose(inverse(M))*p
	XMMATRIX mat = XMLoadFloat4x4(&WorldToObject());

	XMStoreFloat3(&mSlicePlanePoint, XMVector3Transform(XMLoadFloat3(&p), mat));
	XMStoreFloat3(&mSlicePlaneNormal, XMVector3TransformNormal(XMLoadFloat3(&n), mat));
}

void VolumeRenderer::GatherRenderJobs(std::shared_ptr<CommandList> commandList, shared_ptr<Camera> camera, jvector<RenderJob*> &list) {
	if (!mCubeMesh || !mShader) return;
	UpdateTransform();
	
	shared_ptr<ConstantBuffer> cb;
	if (mCBuffers.count(camera.get()))
		cb = mCBuffers.at(camera.get());
	else {
		cb = shared_ptr<ConstantBuffer>(new ConstantBuffer(sizeof(XMFLOAT4X4) * 4, mName + L" CB", Graphics::BufferCount()));
		mCBuffers.emplace(camera.get(), cb);
	}

	XMMATRIX v = XMLoadFloat4x4(&camera->View());
	XMFLOAT4X4 v2o;
	XMStoreFloat4x4(&v2o, XMMatrixMultiply(XMMatrixInverse(&XMMatrixDeterminant(v), v), XMLoadFloat4x4(&WorldToObject())));
	XMFLOAT4X4 o2v;
	XMStoreFloat4x4(&o2v, XMMatrixMultiply(XMLoadFloat4x4(&ObjectToWorld()), v));

	cb->WriteFloat4x4(o2v,					0,						commandList->GetFrameIndex());
	cb->WriteFloat4x4(WorldToObject(),		sizeof(XMFLOAT4X4),		commandList->GetFrameIndex());
	cb->WriteFloat4x4(v2o,					sizeof(XMFLOAT4X4) * 2, commandList->GetFrameIndex());
	cb->WriteFloat4x4(camera->Projection(), sizeof(XMFLOAT4X4) * 3, commandList->GetFrameIndex());

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

	list.push_back(new VolumeRenderJob(5000, this, camera.get(), tbl.get(), cb->GetGPUAddress(commandList->GetFrameIndex())));
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
	commandList->SetMaterial(nullptr);

	if (mVolume->mLightingEnable)
		commandList->EnableKeyword("LIGHTING");
	else
		commandList->DisableKeyword("LIGHTING");

	if (mVolume->mSlicePlaneEnable)
		commandList->EnableKeyword("PLANE");
	else
		commandList->DisableKeyword("PLANE");

	if (mVolume->mISOEnable)
		commandList->EnableKeyword("ISO");
	else
		commandList->DisableKeyword("ISO");

	XMFLOAT4X4 proj = mCamera->Projection();

	float n = mCamera->Near();
	float f = mCamera->Far();

	struct RootData {
		XMFLOAT3 camPos;
		float proj43;
		XMFLOAT3 lightDir;
		float proj33;
		XMFLOAT3 slicep;
		float density;
		XMFLOAT3 slicen;
		float lightDensity;
		float exposure;
	};
	RootData data;
	data.camPos = mCamera->WorldPosition();
	data.proj43 = proj._43;
	XMStoreFloat3(&data.lightDir, XMVector3Rotate(XMLoadFloat3(&mVolume->mLightDir), XMQuaternionInverse(XMLoadFloat4(&mVolume->WorldRotation()))));
	data.proj33 = proj._33;
	data.slicep = mVolume->mSlicePlanePoint;
	data.density = mVolume->mDensity,
	data.slicen = mVolume->mSlicePlaneNormal;
	data.lightDensity = mVolume->mLightDensity;
	data.exposure = mVolume->mExposure;

	commandList->SetShader(mVolume->mShader);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, (UINT)sizeof(RootData) / 4, &data, 0);

	commandList->SetRootDescriptorTable(1, mSRVTable->D3DHeap().Get(), mSRVTable->GpuDescriptor());
	commandList->SetRootCBV(2, mCB);

	commandList->SetBlendState(BLEND_STATE_ALPHA);
	commandList->SetCullMode(D3D12_CULL_MODE_FRONT);
	commandList->DepthTestEnabled(false);
	commandList->DepthWriteEnabled(false);
	commandList->DrawMesh(*mVolume->mCubeMesh);

	commandList->PopState();
}