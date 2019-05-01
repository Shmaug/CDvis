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

	struct RootData {
		DirectX::XMFLOAT3 camPos;	// float3 CameraPosition;
		float proj43;				// float  Projection43;
		DirectX::XMFLOAT3 lightPos;	// float3 LightPosition;
		float proj33;				// float  Projection33;
		DirectX::XMFLOAT3 slicep;	// float3 PlanePoint;
		float density;				// float  Density;
		DirectX::XMFLOAT3 slicen;	// float3 PlaneNormal;
		float lightDensity;			// float  LightDensity;
		DirectX::XMFLOAT3 lightDir;	// float3 LightDirection;
		float exposure;				// float  Exposure;
		DirectX::XMFLOAT3 scale;	// float3 WorldScale;
		float lightIntensity;		// float  LightIntensity;
		float lightAngle;			// float  LightAngle;
		float lightAmbient;			// float  LightAmbient;
		float isoValue;				// float  TresholdValue;
	};

	class VolumeRenderJob : public RenderJob {
	public:
		VolumeRenderer* mVolume;
		Camera* mCamera;
		DescriptorTable* mSRVTable;
		D3D12_GPU_VIRTUAL_ADDRESS mCB;
		VolumeRenderer::RootData& mRootData;

		VolumeRenderJob(unsigned int queue, VolumeRenderer* vol, Camera* camera, DescriptorTable* tbl, D3D12_GPU_VIRTUAL_ADDRESS cb)
			: RenderJob(queue), mVolume(vol), mCamera(camera), mSRVTable(tbl), mCB(cb), mRootData(vol->mRootData) {
			mName = L"VolumeRenderer";
		}

		void Execute(const std::shared_ptr<CommandList>& commandList, const std::shared_ptr<Material>& materialOverride) override;
	};

	VolumeRenderer();
	VolumeRenderer(jwstring name);
	~VolumeRenderer();
	
	bool mSlicePlaneEnable;
	MASK_MODE mMaskMode;
	bool mInvertDensity;
	bool mLightingEnable;

	float mDensity;
	float mLightDensity;
	float mExposure;
	float mThreshold;

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

	std::shared_ptr<Texture> GetTexture() const { return mTexture; }
	void SetTexture(const std::shared_ptr<Texture>& tex);

	void FillMask(float value);
	void PaintMask(const DirectX::XMFLOAT3& lastWorldPos, const DirectX::XMFLOAT3& worldPos, float value, float radius);

	void SetRootParameters(const std::shared_ptr<CommandList>& commandList, Camera* camera);
	void ComputeLighting();

	void GatherRenderJobs(const std::shared_ptr<CommandList>& commandList, const std::shared_ptr<Camera>& camera, jvector<RenderJob*> &list) override;
	bool Visible() override { return mVisible; }
	bool Draggable() override { return true; }

	DirectX::BoundingOrientedBox Bounds() override;

	bool mVisible;

private:
	RootData mRootData;

	DirectX::XMFLOAT3 mSlicePlanePoint;
	DirectX::XMFLOAT3 mSlicePlaneNormal;

	std::shared_ptr<DescriptorTable> mUAVTable;
	std::shared_ptr<Texture> mTexture;
	std::shared_ptr<Texture> mBakedTexture;

	std::shared_ptr<Mesh> mCubeMesh;
	std::unordered_map<Camera*, std::shared_ptr<ConstantBuffer>> mCBuffers;
	std::unordered_map<Camera*, std::shared_ptr<DescriptorTable>*> mSRVTables;
	std::shared_ptr<Shader> mShader;

	std::shared_ptr<Shader> mComputeShader;
};

