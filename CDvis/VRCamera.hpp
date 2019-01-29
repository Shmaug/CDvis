#pragma once

#include <wrl.h>
#include <Camera.hpp>
#include <CommandList.hpp>
#include <openvr.h>

class VRCamera : public Object {
public:
	VRCamera(jwstring name);
	~VRCamera();

	void CreateCameras(unsigned int resx, unsigned int resy);
	void UpdateCameras(vr::IVRSystem* hmd);
	void UpdateCameras(float fov, float separation);

	std::shared_ptr<Camera> LeftEye() const { return mLeftEye; }
	std::shared_ptr<Camera> RightEye() const { return mRightEye; }

	_WRL::ComPtr<ID3D12DescriptorHeap> SRVHeap() const { return mSRVHeap; }
	D3D12_GPU_DESCRIPTOR_HANDLE LeftEyeSRV() const { return mLeftEyeSRV; }
	D3D12_GPU_DESCRIPTOR_HANDLE RightEyeSRV() const { return mRightEyeSRV; }
	_WRL::ComPtr<ID3D12Resource> LeftEyeTexture() const { return mLeftEyeTexture; }
	_WRL::ComPtr<ID3D12Resource> RightEyeTexture() const { return mRightEyeTexture; }

	void ResolveEyeTextures(std::shared_ptr<CommandList> commandList);

	bool UpdateTransform();

private:
	std::shared_ptr<Camera> mLeftEye;
	std::shared_ptr<Camera> mRightEye;

	_WRL::ComPtr<ID3D12Resource> mLeftEyeTexture;
	_WRL::ComPtr<ID3D12Resource> mRightEyeTexture;

	_WRL::ComPtr<ID3D12DescriptorHeap> mSRVHeap;
	D3D12_GPU_DESCRIPTOR_HANDLE mLeftEyeSRV;
	D3D12_GPU_DESCRIPTOR_HANDLE mRightEyeSRV;
};

