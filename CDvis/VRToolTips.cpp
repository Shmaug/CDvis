#include "VRToolTips.hpp"

#include <Graphics.hpp>

using namespace std;
using namespace DirectX;

VRToolTips::VRToolTips(const jwstring& name) : MeshRenderer(name) {
	shared_ptr<Mesh> mesh = shared_ptr<Mesh>(new Mesh(L"ToolTipMesh"));
	mesh->HasSemantic(MESH_SEMANTIC_TEXCOORD0, true);
	mesh->VertexCount(12);

	auto v = mesh->GetVertices();
	auto t = mesh->GetTexcoords(0);

	XMFLOAT3 m = { 0,  .00500f, -.04920f };
	XMFLOAT3 a = { 0, -.02941f, -.04917f };
	XMFLOAT3 g = { 0, -.01561f, -.08721f };

	float u0 = 513.f / 2048.f;
	float u1 = (513.f + 497.f) / 2048.f;

	float v0 = 543.f / 1024.f;
	float v1 = (543.f + 55.f) / 1024.f;

	v[0] = { m.x, m.y, m.z + .01f };			t[0] = { u0, v0, 0, 0 };
	v[1] = { m.x, m.y, m.z - .01f };			t[1] = { u0, v1, 0, 0 };
	v[2] = { m.x + .1258f, m.y, m.z + .01f };	t[2] = { u1, v0, 0, 0 };
	v[3] = { m.x + .1258f, m.y, m.z - .01f };	t[3] = { u1, v1, 0, 0 };

	v0 = 609.f / 1024.f;
	v1 = (609.f + 55.f) / 1024.f;

	v[4] = { a.x, a.y, a.z + .01f };			t[4] = { u0, v0, 0, 0 };
	v[5] = { a.x, a.y, a.z - .01f };			t[5] = { u0, v1, 0, 0 };
	v[6] = { a.x + .1258f, a.y, a.z + .01f };	t[6] = { u1, v0, 0, 0 };
	v[7] = { a.x + .1258f, a.y, a.z - .01f };	t[7] = { u1, v1, 0, 0 };

	v0 = 678.f / 1024.f;
	v1 = (678.f + 55.f) / 1024.f;

	v[8] = { g.x, g.y, g.z + .01f };			t[8] = { u0, v0, 0, 0 };
	v[9] = { g.x, g.y, g.z - .01f };			t[9] = { u0, v1, 0, 0 };
	v[10] = { g.x + .1258f, g.y, g.z + .01f };	t[10] = { u1, v0, 0, 0 };
	v[11] = { g.x + .1258f, g.y, g.z - .01f };	t[11] = { u1, v1, 0, 0 };

	const unsigned int inds[18]{
		0u, 2u, 1u, 1u, 2u, 3u,
		4u, 6u, 5u, 5u, 6u, 7u,
		8u, 10u, 9u, 9u, 10u, 11u,
	};
	mesh->AddIndices(18, inds);

	mesh->UploadStatic();

	SetMesh(mesh);
}