#include "VRUtil.hpp"

using namespace DirectX;

XMFLOAT4X4 VR2DX(const vr::HmdMatrix34_t& mat) {
	return XMFLOAT4X4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], 0.0,
		mat.m[0][1], mat.m[1][1], mat.m[2][1], 0.0,
		mat.m[0][2], mat.m[1][2], mat.m[2][2], 0.0,
		mat.m[0][3], mat.m[1][3], mat.m[2][3], 1.0f
	);
}

void VR2DX(const vr::HmdMatrix34_t& mat, XMFLOAT3 &position, XMFLOAT4 &rotation) {
	XMVECTOR scale;
	XMVECTOR pos;
	XMVECTOR rot;
	XMMatrixDecompose(&scale, &rot, &pos, XMLoadFloat4x4(&VR2DX(mat)));

	XMStoreFloat3(&position, pos);
	XMStoreFloat4(&rotation, rot);

	// rh coordinates to lh coordinates: flip z
	position.z = -position.z;
	rotation.x = -rotation.x;
	rotation.y = -rotation.y;
}

void VR2DX(const vr::HmdMatrix34_t& mat, Object* obj) {
	XMFLOAT3 pos;
	XMFLOAT4 rot;
	VR2DX(mat, pos, rot);
	obj->LocalPosition(pos);
	obj->LocalRotation(rot);
}