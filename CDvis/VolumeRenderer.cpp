#include "VolumeRenderer.hpp"
#include <CommandQueue.hpp>
#include <CommandList.hpp>
#include <AssetDatabase.hpp>
#include <Camera.hpp>
#include <Graphics.hpp>

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

VolumeRenderer::VolumeRenderer() : VolumeRenderer(L"volume") {}
VolumeRenderer::VolumeRenderer(jwstring name) : Renderer(name),
	mDensity(2.f), mLightDensity(100.0f), mLightIntensity(1.f), mExposure(1.f), mISOValue(.2f), mLightMode(-1), mVisible(true),
	mMaskMode(MASK_MODE_ENABLED) {

	mCubeMesh = shared_ptr<Mesh>(new Mesh(L"Cube"));
	mCubeMesh->LoadCube(.5f);
	mCubeMesh->UploadStatic();
	mShader = AssetDatabase::GetAsset<Shader>(L"volume");
	mComputeShader = AssetDatabase::GetAsset<Shader>(L"volume_compute");

	uint16_t white16[2] = { 0xFFFF, 0xFFFF };
	mTexture = shared_ptr<Texture>(new Texture(L"Blank Vol Texture", 1, 1, 1, D3D12_RESOURCE_DIMENSION_TEXTURE3D, 1, DXGI_FORMAT_R16G16_UNORM, 1, white16, 2 * sizeof(uint16_t), false));
	mTexture->Upload(D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}
VolumeRenderer::~VolumeRenderer() {
	for (const auto &i : mSRVTables)
		delete[] i.second;
}

void VolumeRenderer::SetTexture(const std::shared_ptr<Texture>& tex) {
	mTexture = tex;
	if (!mTexture) return;
	for (const auto &it : mSRVTables)
		for (unsigned int i = 0; i < Graphics::BufferCount(); i++) {
			it.second[i]->SetSRV(0, mTexture);
		}
}

void VolumeRenderer::WorldToUVW(const XMFLOAT3& wp, XMFLOAT3& uvw) {
	XMVECTOR lpv = XMVector3Transform(XMLoadFloat3(&wp), XMLoadFloat4x4(&WorldToObject()));
	lpv = XMVectorAdd(lpv, XMVectorSet(.5f, .5f, .5f, 0.f)); // [-.5, .5] to [0, 1]
	XMStoreFloat3(&uvw, lpv);
}

float VolumeRenderer::GetPixel(const XMUINT3& p) const {
	if (p.x < 0 || p.y < 0 || p.z < 0 || p.x >= mTexture->Width() || p.y >= mTexture->Height() || p.z >= mTexture->Depth()) return 0.0f;
	
	uint8_t* pixels = (uint8_t*)mTexture->GetPixelData();
	unsigned int i = p.x + p.y * mTexture->Width() + p.z * (mTexture->Width() * mTexture->Height());

	float x;
	switch (mTexture->Format()) {
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	{
		XMFLOAT4 f{ (float)pixels[i * 4] / 255.f, (float)pixels[i * 4 + 1] / 255.f, (float)pixels[i * 4 + 2] / 255.f, (float)pixels[i * 4 + 3] / 255.f };
		x = sqrtf(f.x * f.x + f.y * f.y);
		break;
	}

	case DXGI_FORMAT_R8_UNORM:
		x = (float)pixels[i] / 255.f;
		break;

	case DXGI_FORMAT_R16_UNORM:
		x = (float)((uint16_t*)pixels)[i] / 65535.f;
		break;

	case DXGI_FORMAT_R16G16_UNORM:
		x = ((float)((uint16_t*)pixels)[i * 2] / 65535.f);
		if (mMaskMode == MASK_MODE_ENABLED)
			x *= ((float)((uint16_t*)pixels)[i * 2 + 1] / 65535.f);
		break;

	case DXGI_FORMAT_R32_FLOAT:
		x = ((float*)pixels)[i];
		break;
	}

	if (mISOEnable) x = fmaxf(0, x - mISOValue);
	return x;
}

float VolumeRenderer::GetDensity(const XMFLOAT3& uvw, bool checkPlane) const {
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
float VolumeRenderer::GetDensityTrilinear(const XMFLOAT3& uvw, bool checkPlane) const {
	if (checkPlane && mSlicePlaneEnable) {
		XMVECTOR p = XMVectorSubtract(XMLoadFloat3(&uvw), XMVectorSet(.5f, .5f, .5f, 0.f));
		if (XMVectorGetX(XMVector3Dot(XMVectorSubtract(p, XMLoadFloat3(&mSlicePlanePoint)), XMLoadFloat3(&mSlicePlaneNormal))) < 0)
			return 0.f;
	}

	float x = uvw.x * mTexture->Width();
	float y = uvw.y * mTexture->Height();
	float z = uvw.z * mTexture->Depth();

	float fx = x - (int)x;
	float fy = y - (int)y;
	float fz = z - (int)z;

	XMUINT3 p { (unsigned int)x, (unsigned int)y, (unsigned int)z };

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

void VolumeRenderer::SetSlicePlane(const DirectX::XMFLOAT3& p, const DirectX::XMFLOAT3& n) {
	// p' = transpose(inverse(M))*p
	XMMATRIX mat = XMLoadFloat4x4(&WorldToObject());

	XMStoreFloat3(&mSlicePlanePoint, XMVector3Transform(XMLoadFloat3(&p), mat));
	XMStoreFloat3(&mSlicePlaneNormal, XMVector3Rotate(XMLoadFloat3(&n), XMQuaternionInverse(XMLoadFloat4(&WorldRotation()))));
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

void VolumeRenderer::FillMask(float value) {
	if (!mComputeShader || !mTexture) return;

	auto commandQueue = Graphics::GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
	auto commandList = commandQueue->GetCommandList(0);

	commandList->EnableKeyword("FILL");
	commandList->SetCompute(mComputeShader);

	commandList->D3DCommandList()->SetComputeRoot32BitConstants(1, 1, &value, 3);
	commandList->SetComputeRootDescriptorTable(0, mTexture->GetUAVDescriptorHeap().Get(), mTexture->GetUAVGPUDescriptor());
	commandList->D3DCommandList()->Dispatch((mTexture->Width() + 7) / 8, (mTexture->Height() + 7) / 8, (mTexture->Depth() + 7) / 8);
	
	commandQueue->Execute(commandList);
}
void VolumeRenderer::PaintMask(const XMFLOAT3& lastWorldPos, const XMFLOAT3& worldPos, float value, float radius) {
	if (!mComputeShader || !mTexture) return;

	struct Data {
		XMFLOAT4X4 indexToWorld;
		XMFLOAT3 paintPos0;
		float paintRadius;
		XMFLOAT3 paintPos1;
		float paintValue;
	};
	Data d;
	d.paintPos0 = lastWorldPos;
	d.paintRadius = radius * radius;
	d.paintPos1 = worldPos;
	d.paintValue = value;

	XMStoreFloat4x4(&d.indexToWorld,
		XMMatrixMultiply(
			XMMatrixAffineTransformation(
				XMVectorSet(1.f / (float)mTexture->Width(), 1.f / (float)mTexture->Height(), 1.f / (float)mTexture->Depth(), 1.f),
				XMVectorZero(), XMQuaternionIdentity(),
				XMVectorSet(-.5f, -.5f, -.5f, 0.f)),
			XMLoadFloat4x4(&ObjectToWorld())));

	auto commandQueue = Graphics::GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
	auto commandList = commandQueue->GetCommandList(0);

	commandList->EnableKeyword("PAINT");
	commandList->SetCompute(mComputeShader);

	commandList->D3DCommandList()->SetComputeRoot32BitConstants(1, sizeof(Data) / 4, &d, 0);
	commandList->SetComputeRootDescriptorTable(0, mTexture->GetUAVDescriptorHeap().Get(), mTexture->GetUAVGPUDescriptor());

	commandList->D3DCommandList()->Dispatch((mTexture->Width() + 7) / 8, (mTexture->Height() + 7) / 8, (mTexture->Depth() + 7) / 8);

	commandQueue->Execute(commandList);
}

void VolumeRenderer::GatherRenderJobs(const std::shared_ptr<CommandList>& commandList, const shared_ptr<Camera>& camera, jvector<RenderJob*> &list) {
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
			arr[i] = shared_ptr<DescriptorTable>(new DescriptorTable(mName + L" SRV Table", 3));
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

void VolumeRenderer::VolumeRenderJob::Execute(const shared_ptr<CommandList>& commandList, const shared_ptr<Material>& materialOveride) {
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

	if (mVolume->mLightingEnable) {
		switch (mVolume->mLightMode) {
		case 0:
			commandList->EnableKeyword("LIGHT_DIRECTIONAL");
			break;
		case 1:
			commandList->EnableKeyword("LIGHT_SPOT");
			break;
		case 2:
			commandList->EnableKeyword("LIGHT_POINT");
			break;
		}
	}

	if (mVolume->mSlicePlaneEnable)
		commandList->EnableKeyword("PLANE");
	else
		commandList->DisableKeyword("PLANE");

	if (mVolume->mISOEnable)
		commandList->EnableKeyword("ISO");
	else
		commandList->DisableKeyword("ISO");

	if (mVolume->mTexture->Format() == DXGI_FORMAT_R8G8B8A8_UNORM) {
		commandList->EnableKeyword("COLOR");
		commandList->DisableKeyword("MASK");
		commandList->DisableKeyword("MASK_DISP");
	} else {
		commandList->DisableKeyword("COLOR");

		switch (mVolume->mMaskMode) {
		default:
			commandList->DisableKeyword("MASK");
			commandList->DisableKeyword("MASK_DISP");
			break;
		case MASK_MODE_ENABLED:
			commandList->EnableKeyword("MASK");
			break;
		case MASK_MODE_DISPLAY:
			commandList->EnableKeyword("MASK_DISP");
			break;
		}
	}

	XMMATRIX w2o = XMLoadFloat4x4(&mVolume->WorldToObject());
	XMFLOAT4X4 proj = mCamera->Projection();

	float n = mCamera->Near();
	float f = mCamera->Far();

	XMVECTOR invRot = XMQuaternionInverse(XMLoadFloat4(&mVolume->WorldRotation()));

	struct RootData {
		XMFLOAT3 camPos;		// float3 CameraPosition;
		float proj43;			// float Projection43;
		XMFLOAT3 lightPos;		// float3 LightPosition;
		float proj33;			// float Projection33;
		XMFLOAT3 slicep;		// float3 PlanePoint;
		float density;			// float Density;
		XMFLOAT3 slicen;		// float3 PlaneNormal;
		float lightDensity;		// float LightDensity;
		XMFLOAT3 lightDir;		// float3 LightDirection;
		float exposure;			// float Exposure;
		XMFLOAT3 scale;			// float3 WorldScale;
		float lightIntensity;	// float LightIntensity;
		float lightAngle;		// float LightAngle;
		float lightAmbient;		// float LightAmbient;
		float isoValue;			// float ISOValue;
	};
	RootData data;

	data.camPos = mCamera->WorldPosition();
	data.proj43 = proj._43;
	XMStoreFloat3(&data.lightPos,
		XMVector3Rotate(XMVectorSubtract(XMLoadFloat3((XMFLOAT3*)&mVolume->mLightPos), XMLoadFloat3(&mVolume->WorldPosition())), invRot));
	data.proj33 = proj._33;
	data.slicep = mVolume->mSlicePlanePoint;
	data.density = mVolume->mDensity,
	data.slicen = mVolume->mSlicePlaneNormal;
	data.lightDensity = mVolume->mLightDensity;
	XMStoreFloat3(&data.lightDir, XMVector3Rotate(XMLoadFloat3(&mVolume->mLightDir), invRot));
	data.exposure = mVolume->mExposure;
	data.scale = mVolume->LocalScale();
	data.lightIntensity = mVolume->mLightIntensity;
	data.lightAngle = mVolume->mLightAngle;
	data.lightAmbient = mVolume->mLightAmbient;
	data.isoValue = mVolume->mISOValue;

	commandList->SetShader(mVolume->mShader);
	commandList->D3DCommandList()->SetGraphicsRoot32BitConstants(0, (UINT)sizeof(RootData) / 4, &data, 0);

	commandList->SetGraphicsRootDescriptorTable(1, mSRVTable->D3DHeap().Get(), mSRVTable->GpuDescriptor());
	commandList->SetRootCBV(2, mCB);

	commandList->SetBlendState(BLEND_STATE_ALPHA);
	commandList->SetCullMode(D3D12_CULL_MODE_FRONT);
	commandList->DepthTestEnabled(false);
	commandList->DepthWriteEnabled(false);
	commandList->DrawMesh(*mVolume->mCubeMesh);

	commandList->PopState();
}