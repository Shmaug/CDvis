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
	enum MASK_MODE {
		MASK_MODE_ENABLED,
		MASK_MODE_DISPLAY,
		MASK_MODE_DISABLED
	};

	class VolumeRenderJob : public RenderJob {
	public:
		VolumeRenderer* mVolume;
		Camera* mCamera;
		DescriptorTable* mSRVTable;
		D3D12_GPU_VIRTUAL_ADDRESS mCB;

		VolumeRenderJob(unsigned int queue, VolumeRenderer* vol, Camera* camera, DescriptorTable* tbl, D3D12_GPU_VIRTUAL_ADDRESS cb)
			: RenderJob(queue), mVolume(vol), mCamera(camera), mSRVTable(tbl), mCB(cb) {}

		void Execute(const std::shared_ptr<CommandList>& commandList, const std::shared_ptr<Material>& materialOverride) override;
	};

	VolumeRenderer();
	VolumeRenderer(jwstring name);
	~VolumeRenderer();
	
	bool mSlicePlaneEnable;
	MASK_MODE mMaskMode;
	bool mISOEnable;
	bool mInvertDensity;
	bool mLightingEnable;

	float mDensity;
	float mLightDensity;
	float mExposure;
	float mISOValue;

	DirectX::XMFLOAT3 mLightPos;
	DirectX::XMFLOAT3 mLightDir;
	int mLightMode;
	float mLightIntensity;
	float mLightAngle;
	float mLightRange;
	float mLightAmbient;

	void SetSlicePlane(const DirectX::XMFLOAT3& p, const DirectX::XMFLOAT3& n);

	void WorldToUVW(const DirectX::XMFLOAT3& wp, DirectX::XMFLOAT3& uvw);

	float GetPixel(const DirectX::XMUINT3& p) const;
	float GetDensity(const DirectX::XMFLOAT3& uvw, bool slicePlane = false) const;
	float GetDensityTrilinear(const DirectX::XMFLOAT3& uvw, bool slicePlane = false) const;

	void SetTexture(const std::shared_ptr<Texture>& tex);

	void FillMask(float value);
	void PaintMask(const DirectX::XMFLOAT3& lastWorldPos, const DirectX::XMFLOAT3& worldPos, float value, float radius);

	void GatherRenderJobs(const std::shared_ptr<CommandList>& commandList, const std::shared_ptr<Camera>& camera, jvector<RenderJob*> &list) override;
	bool Visible() override { return mVisible; }
	bool Draggable() override { return true; }

	DirectX::BoundingOrientedBox Bounds() override;

	bool mVisible;

private:
	DirectX::XMFLOAT3 mSlicePlanePoint;
	DirectX::XMFLOAT3 mSlicePlaneNormal;

	std::shared_ptr<Texture> mTexture;
	std::shared_ptr<Mesh> mCubeMesh;
	std::unordered_map<Camera*, std::shared_ptr<ConstantBuffer>> mCBuffers;
	std::unordered_map<Camera*, std::shared_ptr<DescriptorTable>*> mSRVTables;
	std::shared_ptr<Shader> mShader;

	std::shared_ptr<Shader> mComputeShader;
};

