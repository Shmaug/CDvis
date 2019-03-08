#include "VRPieMenu.hpp"

#include <AssetDatabase.hpp>
#include <Graphics.hpp>
#include <Shader.hpp>

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

constexpr unsigned int PieResolution = 64;

VRPieMenu::VRPieMenu() : VRPieMenu(L"") {}
VRPieMenu::VRPieMenu(jwstring name) : Renderer(name), mVisible(true), mRadius(.1f), mSliceCount(4), mPressedSlice(-1), mHoveredSlice(-1) {
	mCBuffer = shared_ptr<ConstantBuffer>(new ConstantBuffer(sizeof(XMFLOAT4X4) * 2, L"PieMenu CB", Graphics::BufferCount()));
	mMeshes = new PieMesh[Graphics::BufferCount()];
	ZeroMemory(mMeshes, sizeof(PieMesh) * Graphics::BufferCount());
}
VRPieMenu::~VRPieMenu() { delete[] mMeshes; }

void VRPieMenu::Initialize(float radius, unsigned sliceCount, const shared_ptr<Texture>& icons) {
	mMaterial = shared_ptr<Material>(new Material(L"Pie Menu", AssetDatabase::GetAsset<Shader>(L"piemenu")));
	mMaterial->CullMode(D3D12_CULL_MODE_NONE);
	mMaterial->Blend(BLEND_STATE_ALPHA);
	mMaterial->SetTexture("Texture", icons, -1);
	mRadius = radius;
	mSliceCount = sliceCount;
	mIconTexture = icons;
}

VRPieMenu::PieMesh::~PieMesh() {
	mVertexBuffer.Reset();
	mIndexBuffer.Reset();
}

void VRPieMenu::UpdateMesh(PieMesh& mesh) {
	unsigned int sliceResolution = (PieResolution + (mSliceCount - 1)) / mSliceCount; // round up so each slice has the same number of triangles

	mesh.mIndexCount = 3 * (sliceResolution - 1) * mSliceCount;
	mesh.mIconIndexCount = mSliceCount * 6;

	if (!mesh.mMapped) {
		auto device = Graphics::GetDevice();

		unsigned int vertexCount = sliceResolution * mSliceCount + 2 + mSliceCount * 4;
		unsigned int indexCount = mesh.mIndexCount + mesh.mIconIndexCount;

		// Vertices
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexCount * sizeof(PieVertex)),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&mesh.mVertexBuffer)));

		mesh.mVertexBuffer->SetName(L"Pie Vertex Buffer");

		void* vdata;
		CD3DX12_RANGE readRange(0, 0);
		mesh.mVertexBuffer->Map(0, &readRange, &vdata);

		mesh.mVertexBufferView.BufferLocation = mesh.mVertexBuffer->GetGPUVirtualAddress();
		mesh.mVertexBufferView.SizeInBytes = (UINT)vertexCount * sizeof(PieVertex);
		mesh.mVertexBufferView.StrideInBytes = sizeof(PieVertex);

		mesh.mVertices = reinterpret_cast<PieVertex*>(vdata);

		// Indices
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(indexCount * sizeof(uint16_t)),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&mesh.mIndexBuffer)));

		mesh.mIndexBuffer->SetName(L"Pie Index Buffer");

		void* idata;
		mesh.mIndexBuffer->Map(0, &readRange, &idata);

		mesh.mIndexBufferView.BufferLocation = mesh.mIndexBuffer->GetGPUVirtualAddress();
		mesh.mIndexBufferView.SizeInBytes = (UINT)indexCount * sizeof(uint16_t);
		mesh.mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;

		mesh.mIndices = reinterpret_cast<uint16_t*>(idata);

		mesh.mMapped = true;
	}

	mesh.mVertices[0] = { { 0, 0, 0 }, { 0, 0, 0, .5f }, { 0, 0, 0, 0 } };
	unsigned int i = 0;
	unsigned int j = 1;

	for (unsigned int s = 0; s < mSliceCount; s++) {
		XMFLOAT4 c = { 0, 0, 0, .75f };
		if (s == mPressedSlice)
			c.x = c.y = c.z = .25f;
		else if (s == mHoveredSlice)
			c.x = c.y = c.z = .1f;

		float to = XM_2PI * s / mSliceCount - XM_PIDIV2;
		for (unsigned int t = 0; t < sliceResolution; t++) {
			float theta = (XM_2PI / mSliceCount) * (float)t / (float)(sliceResolution - 1);
			float x = cosf(theta + to);
			float z = sinf(theta + to);

			if (t > 0) {
				mesh.mIndices[i++] = 0;
				mesh.mIndices[i++] = j;
				mesh.mIndices[i++] = j - 1;
			}

			mesh.mVertices[j++] = { { x * mRadius, 0, z * mRadius }, c, { x, z, 0, 0 } };
		}
	}

	for (unsigned int s = 0; s < mSliceCount; s++) {
		mesh.mIndices[i++] = j;
		mesh.mIndices[i++] = j + 1;
		mesh.mIndices[i++] = j + 2;

		mesh.mIndices[i++] = j;
		mesh.mIndices[i++] = j + 2;
		mesh.mIndices[i++] = j + 3;

		XMFLOAT4 c = { 1, 1, 1, 1 };
		float y = 0.005f;

		float rp = mRadius * .5f;

		float theta = XM_2PI * (s + .5f) / mSliceCount - XM_PIDIV2;

		float x = cosf(theta) * rp;
		float z = sinf(theta) * rp;

		float rm = rp * cosf(XM_PIDIV2 - XM_2PI / mSliceCount * .5f);
		float b = 0.70710678118f * rm;

		float u0 = (float)s / mSliceCount;
		float u1 = u0 + 1.f / (float)mSliceCount;

		mesh.mVertices[j++] = { { x - b, y, z + b }, c, { u0, 0, 0, 0 } };
		mesh.mVertices[j++] = { { x + b, y, z + b }, c, { u1, 0, 0, 0 } };
		mesh.mVertices[j++] = { { x + b, y, z - b }, c, { u1, 1, 0, 0 } };
		mesh.mVertices[j++] = { { x - b, y, z - b }, c, { u0, 1, 0, 0 } };
	}
}
bool VRPieMenu::UpdateTouch(const XMFLOAT2& touchPos) {
	mTouchPos = touchPos;
	float angle = atan2f(touchPos.y, touchPos.x) + XM_PIDIV2;;
	if (angle < 0) angle += XM_2PI;
	if (angle > XM_2PI) angle -= XM_2PI;
	float interval = XM_2PI / mSliceCount;
	int s = (int)(angle / interval);
	if (s != mHoveredSlice) {
		mHoveredSlice = s;
		return true;
	}
	return false;
}

DirectX::BoundingOrientedBox VRPieMenu::Bounds() {
	return BoundingOrientedBox(WorldPosition(), XMFLOAT3(mRadius, mRadius, 0.01f), WorldRotation());
}

void VRPieMenu::GatherRenderJobs(const std::shared_ptr<CommandList>& commandList, const shared_ptr<Camera>& camera, jvector<RenderJob*> &list) {
	UpdateTransform();
	UpdateMesh(mMeshes[commandList->GetFrameIndex()]);

	mCBuffer->WriteFloat4x4(ObjectToWorld(), 0, commandList->GetFrameIndex());
	mCBuffer->WriteFloat4x4(WorldToObject(), sizeof(XMFLOAT4X4), commandList->GetFrameIndex());
	mMaterial->SetCBuffer("ObjectBuffer", mCBuffer, commandList->GetFrameIndex());
	mMaterial->SetFloat2("TouchPos", mTouchPos, commandList->GetFrameIndex());

	list.push_back(new PieMenuRenderJob(2000, mMeshes[commandList->GetFrameIndex()], mMaterial));
}

void VRPieMenu::PieMenuRenderJob::Execute(const shared_ptr<CommandList>& commandList, const shared_ptr<Material>& materialOveride) {
	if (materialOveride) return;

	commandList->PushState();

	mMaterial->DisableKeyword("TEXTURED");
	mMaterial->ZWrite(true);
	commandList->SetMaterial(mMaterial);
	commandList->DrawUserMesh((MESH_SEMANTIC)(MESH_SEMANTIC_POSITION | MESH_SEMANTIC_COLOR0 | MESH_SEMANTIC_TEXCOORD0));
	commandList->Draw(mMesh.mVertexBufferView, mMesh.mIndexBufferView, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, mMesh.mIndexCount, 0, 0);

	mMaterial->EnableKeyword("TEXTURED");
	mMaterial->ZWrite(false);
	commandList->SetMaterial(mMaterial);
	commandList->DrawUserMesh((MESH_SEMANTIC)(MESH_SEMANTIC_POSITION | MESH_SEMANTIC_COLOR0 | MESH_SEMANTIC_TEXCOORD0));
	commandList->Draw(mMesh.mVertexBufferView, mMesh.mIndexBufferView, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, mMesh.mIconIndexCount, mMesh.mIndexCount, 0);

	commandList->PopState();
}