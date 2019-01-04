#include "VRCamera.hpp"

#include <Scene.hpp>
#include <Graphics.hpp>

#include "VRUtil.hpp"

using namespace std;
using namespace DirectX;

VRCamera::VRCamera(jwstring name) : Object(name) {}
VRCamera::~VRCamera() {}

void VRCamera::CreateCameras(unsigned int resx, unsigned int resy){
	mLeftEye = shared_ptr<Camera>(new Camera(mName + L" Left Eye"));
	GetScene()->AddObject(mLeftEye);
	mLeftEye->Parent(shared_from_this());
	mLeftEye->LocalPosition(-.03f, 0, .025f);

	mRightEye = shared_ptr<Camera>(new Camera(mName + L" Right Eye"));
	GetScene()->AddObject(mRightEye);
	mRightEye->Parent(shared_from_this());
	mRightEye->LocalPosition(.03f, 0, .025f);

	mLeftEye->PixelWidth(resx);
	mLeftEye->PixelHeight(resy);
	mLeftEye->CreateRenderBuffers();

	mRightEye->PixelWidth(resx);
	mRightEye->PixelHeight(resy);
	mRightEye->CreateRenderBuffers();

	auto device = Graphics::GetDevice();

	// Create textures
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(mLeftEye->RenderFormat(), mLeftEye->PixelWidth(), mLeftEye->PixelHeight()),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&mLeftEyeTexture)));

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(mRightEye->RenderFormat(), mRightEye->PixelWidth(), mRightEye->PixelHeight()),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&mRightEyeTexture)));

	// Create SRV Heap and SRVs
	auto inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mSRVHeap = Graphics::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = mLeftEye->RenderFormat();
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuhandle(mSRVHeap->GetCPUDescriptorHandleForHeapStart(), 0, inc);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuhandle(mSRVHeap->GetGPUDescriptorHandleForHeapStart(), 0, inc);
	device->CreateShaderResourceView(mLeftEyeTexture.Get(), &srvDesc, cpuhandle);
	mLeftEyeSRV = gpuhandle;
	cpuhandle.Offset(inc);
	gpuhandle.Offset(inc);
	device->CreateShaderResourceView(mRightEyeTexture.Get(), &srvDesc, cpuhandle);
	mRightEyeSRV = gpuhandle;
}
void VRCamera::UpdateCameras(vr::IVRSystem* hmd) {
	VR2DX(hmd->GetEyeToHeadTransform(vr::EVREye::Eye_Left), mLeftEye.get());
	VR2DX(hmd->GetEyeToHeadTransform(vr::EVREye::Eye_Right), mRightEye.get());

	float l, r, t, b;
	hmd->GetProjectionRaw(vr::EVREye::Eye_Left, &l, &r, &t, &b);
	l *=  mLeftEye->Near();
	r *=  mLeftEye->Near();
	t *= -mLeftEye->Near();
	b *= -mLeftEye->Near();
	mLeftEye->PerspectiveBounds(XMFLOAT4(l, r, b, t));

	hmd->GetProjectionRaw(vr::EVREye::Eye_Right, &l, &r, &t, &b);
	l *=  mRightEye->Near();
	r *=  mRightEye->Near();
	t *= -mRightEye->Near();
	b *= -mRightEye->Near();
	mRightEye->PerspectiveBounds(XMFLOAT4(l, r, b, t));
}

bool VRCamera::UpdateTransform() {
	if (!Object::UpdateTransform()) return false;

	mLeftEye->UpdateTransform();
	mRightEye->UpdateTransform();

	return true;
}

void VRCamera::ResolveEyeTextures(shared_ptr<CommandList> commandList) {
	commandList->TransitionResource(mLeftEye->RenderBuffer().Get(),  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	commandList->TransitionResource(mRightEye->RenderBuffer().Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

	commandList->TransitionResource(mLeftEyeTexture.Get(),  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST);
	commandList->TransitionResource(mRightEyeTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST);

	commandList->D3DCommandList()->ResolveSubresource(mLeftEyeTexture.Get(), 0, mLeftEye->RenderBuffer().Get(), 0, mLeftEye->RenderFormat());
	commandList->D3DCommandList()->ResolveSubresource(mRightEyeTexture.Get(), 0, mRightEye->RenderBuffer().Get(), 0, mRightEye->RenderFormat());

	commandList->TransitionResource(mLeftEyeTexture.Get(),  D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->TransitionResource(mRightEyeTexture.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	commandList->TransitionResource(mLeftEye->RenderBuffer().Get(),  D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->TransitionResource(mRightEye->RenderBuffer().Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
}