#include "VRUtil.hpp"

using namespace DirectX;

XMFLOAT4X4 VR2DX(vr::HmdMatrix34_t& mat) {
	return XMFLOAT4X4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], 0.0,
		mat.m[0][1], mat.m[1][1], mat.m[2][1], 0.0,
		mat.m[0][2], mat.m[1][2], mat.m[2][2], 0.0,
		mat.m[0][3], mat.m[1][3], mat.m[2][3], 1.0f
	);
}
void VR2DX(vr::HmdMatrix34_t& mat, Object* obj) {
	XMVECTOR scale;
	XMVECTOR pos;
	XMVECTOR rot;
	XMMatrixDecompose(&scale, &rot, &pos, XMLoadFloat4x4(&VR2DX(mat)));

	XMFLOAT3 pos3;
	XMStoreFloat3(&pos3, pos);
	XMFLOAT4 rot4;
	XMStoreFloat4(&rot4, rot);

	// rh coordinates to lh coordinates: flip z
	pos3.z = -pos3.z;
	rot4.x = -rot4.x;
	rot4.y = -rot4.y;

	obj->LocalPosition(pos3);
	obj->LocalRotation(rot4);
}