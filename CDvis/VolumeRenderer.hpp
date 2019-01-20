#pragma once

#include <Renderer.hpp>
#include <CommandList.hpp>
#include <Mesh.hpp>
#include <Texture.hpp>
#include <DescriptorTable.hpp>
#include <Shader.hpp>

class VolumeRenderer : public Renderer {
public:
	class VolumeRenderJob : public RenderJob {
	public:
		VolumeRenderer* mVolume;
		Camera* mCamera;
		DescriptorTable* mSRVTable;

		VolumeRenderJob(unsigned int queue, VolumeRenderer* vol, Camera* camera, DescriptorTable* tbl)
			: RenderJob(queue), mVolume(vol), mCamera(camera), mSRVTable(tbl) {}

		void Execute(std::shared_ptr<CommandList> commandList, std::shared_ptr<Material> materialOverride) override;
	};

	VolumeRenderer();
	VolumeRenderer(jwstring name);
	~VolumeRenderer();
	
	bool mSlicePlaneEnable;
	bool mDepthColor;
	bool mLightingEnable;

	float mDensity;
	float mLightDensity;

	DirectX::XMFLOAT4 mSlicePlane;

	void SetTexture(std::shared_ptr<Texture> tex);

	void GatherRenderJobs(std::shared_ptr<CommandList> commandList, std::shared_ptr<Camera> camera, jvector<RenderJob*> &list) override;
	bool Visible() override { return mVisible; }

	DirectX::BoundingOrientedBox Bounds() override;

	bool mVisible;

private:
	std::shared_ptr<Texture> mTexture;
	std::unordered_map<Camera*, std::shared_ptr<DescriptorTable>*> mSRVTables;
	std::shared_ptr<Mesh> mCubeMesh;
	std::shared_ptr<Shader> mShader;
};

