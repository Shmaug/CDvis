#pragma once

#include <Renderer.hpp>
#include <CommandList.hpp>
#include <Material.hpp>
#include <Texture.hpp>
#include <Camera.hpp>
#include <ConstantBuffer.hpp>

class VRPieMenu : public Renderer {
private:
	#pragma pack(push, 1)
	struct PieVertex {
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT4 col;
		DirectX::XMFLOAT4 tex;
	};
	#pragma pack(pop)

	struct PieMesh {
		_WRL::ComPtr<ID3D12Resource> mVertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
		PieVertex* mVertices;
		_WRL::ComPtr<ID3D12Resource> mIndexBuffer;
		D3D12_INDEX_BUFFER_VIEW mIndexBufferView;
		uint16_t* mIndices;
		unsigned int mIndexCount;
		unsigned int mIconIndexCount;
		bool mMapped;

		~PieMesh();
	};

public:
	class PieMenuRenderJob : public RenderJob {
	public:
		const PieMesh& mMesh;
		const std::shared_ptr<Material>& mMaterial;

		PieMenuRenderJob(unsigned int queue, const PieMesh& mesh, const std::shared_ptr<Material>& mat)
			: RenderJob(queue), mMesh(mesh), mMaterial(mat) {}

		void Execute(const std::shared_ptr<CommandList>& commandList, const std::shared_ptr<Material>& materialOverride) override;
	};

	bool mVisible;

	VRPieMenu();
	VRPieMenu(jwstring name);
	~VRPieMenu();

	inline int Hovered() const { return mHoveredSlice; }
	inline int Pressed() const { return mPressedSlice; }
	inline void Pressed(int p) { mPressedSlice = p; }

	void Initialize(float radius, unsigned sliceCount, const std::shared_ptr<Texture>& icons);

	bool UpdateTouch(const DirectX::XMFLOAT2& touchPos);

	DirectX::BoundingOrientedBox Bounds() override;
	void GatherRenderJobs(const std::shared_ptr<CommandList>& commandList, const std::shared_ptr<Camera>& camera, jvector<RenderJob*> &list) override;
	bool Visible() override { return mVisible; }

private:
	std::shared_ptr<Texture> mIconTexture;
	std::shared_ptr<Material> mMaterial;
	PieMesh* mMeshes;
	std::shared_ptr<ConstantBuffer> mCBuffer;
	unsigned int mSliceCount;
	int mHoveredSlice;
	int mPressedSlice;
	float mRadius;
	DirectX::XMFLOAT2 mTouchPos;

	void UpdateMesh(PieMesh& mesh);
};

