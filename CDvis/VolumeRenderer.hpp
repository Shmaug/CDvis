#pragma once

#include <Renderer.hpp>
#include <CommandList.hpp>
#include <Mesh.hpp>
#include <Texture.hpp>
#include <DescriptorTable.hpp>
#include <Shader.hpp>
#include <ConstantBuffer.hpp>
#include "VRInteractable.hpp"

class VolumeRenderer : public Renderer, public VRInteractable {
public:
	class VolumeRenderJob : public RenderJob {
	public:
		VolumeRenderer* mVolume;
		Camera* mCamera;
		DescriptorTable* mSRVTable;
		D3D12_GPU_VIRTUAL_ADDRESS mCB;

		VolumeRenderJob(unsigned int queue, VolumeRenderer* vol, Camera* camera, DescriptorTable* tbl, D3D12_GPU_VIRTUAL_ADDRESS cb)
			: RenderJob(queue), mVolume(vol), mCamera(camera), mSRVTable(tbl), mCB(cb) {}

		void Execute(std::shared_ptr<CommandList> commandList, std::shared_ptr<Material> materialOverride) override;
	};

	VolumeRenderer();
	VolumeRenderer(jwstring name);
	~VolumeRenderer();
	
	bool mSlicePlaneEnable;
	bool mISOEnable;
	bool mLightingEnable;

	float mDensity;
	float mLightDensity;
	float mExposure;
	float mISOValue;

	DirectX::XMFLOAT3 mLightPos;
	DirectX::XMFLOAT3 mLightDir;
	int mLightMode;
	float mLightAngle;
	float mLightRange;
	float mLightAmbient;

	void SetSlicePlane(DirectX::XMFLOAT3 p, DirectX::XMFLOAT3 n);

	float GetPixel(DirectX::XMUINT3 p) const;
	float GetDensity(DirectX::XMFLOAT3 uvw, bool slicePlane = false) const;
	float GetDensityTrilinear(DirectX::XMFLOAT3 uvw, bool slicePlane = false) const;

	void SetTexture(std::shared_ptr<Texture> tex);

	void GatherRenderJobs(std::shared_ptr<CommandList> commandList, std::shared_ptr<Camera> camera, jvector<RenderJob*> &list) override;
	bool Visible() override { return mVisible; }
	bool Draggable() override { return true; }

	DirectX::BoundingOrientedBox Bounds() override;

	bool mVisible;

private:
	DirectX::XMFLOAT3 mSlicePlanePoint;
	DirectX::XMFLOAT3 mSlicePlaneNormal;

	std::shared_ptr<Texture> mTexture;
	std::unordered_map<Camera*, std::shared_ptr<DescriptorTable>*> mSRVTables;
	std::unordered_map<Camera*, std::shared_ptr<ConstantBuffer>> mCBuffers;
	std::shared_ptr<Mesh> mCubeMesh;
	std::shared_ptr<Shader> mShader;
};

